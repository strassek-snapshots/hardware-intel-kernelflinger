/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * x86 specific timer routines
 */
#include <efi.h>
#include <efilib.h>
#include <lib.h>
#include "timer.h"

#define BOOT_STAGE_FIRMWARE "FWS"
#define BOOT_STAGE_OSLOADER "OLS"
#define BOOT_STAGE_CHECK_BCB "CBS"
#define BOOT_STAGE_VERIFY_BOOT "VBS"
#define BOOT_STAGE_VERIFY_TRUSTY "VTS"
#define BOOT_STAGE_START_KERNEL "SKS"

//Array for recording boot time of every stage
static unsigned bt_stamp[TM_POINT_LAST];

typedef union
{
	uint64_t val;
	struct
	{
		uint32_t lo;
		uint32_t hi;
	};

} msr_t;

static uint64_t __attribute__((unused,always_inline))
__RDMSR (unsigned idx)
{
	msr_t msr;

	asm volatile ("rdmsr" : "=a" (msr.lo), "=d" (msr.hi) : "c" (idx));
	return msr.val;
}

static uint64_t __attribute__((unused,always_inline))
__RDTSC (void)
{
	uint32_t lo, hi;

	asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
	return (uint64_t) hi << 32 | lo;
}

static uint16_t get_cpu_freq(void)
{
	uint16_t cpu_freq;
	unsigned max_nb_ratio;
	msr_t platform_info;

	platform_info.val = __RDMSR (0xce);
	max_nb_ratio = (platform_info.lo >> 8) & 0xff;
	cpu_freq = 100 * max_nb_ratio;

	return cpu_freq;
}

unsigned boottime_in_msec(void)
{
	unsigned tick;
	unsigned bt_us, bt_ms;
	unsigned cpu_freq;

	cpu_freq = get_cpu_freq();

	tick = __RDTSC();
	bt_us = (((unsigned) (tick >> 6)) / cpu_freq) << 6;
	bt_ms = bt_us / 1000;

	return bt_ms;
}

void set_boottime_stamp(int num)
{
	if ((num < 0) || (num >= TM_POINT_LAST))
		return;

	bt_stamp[num] = boottime_in_msec();
}

void construct_stages_boottime(CHAR8 *time_str, size_t buf_len)
{
	CHAR8 interval_str[16] = {0};

	if (!time_str)
		return;

	strlcat(time_str, (CHAR8 *)BOOT_STAGE_FIRMWARE, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
	itoa(bt_stamp[TM_EFI_MAIN], interval_str, 10);
	strlcat(time_str, interval_str, buf_len);

	strlcat(time_str, (CHAR8 *)",", buf_len);
#ifdef USE_AVB
	strlcat(time_str, (CHAR8 *)BOOT_STAGE_CHECK_BCB, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
	itoa(bt_stamp[TM_AVB_START] - bt_stamp[TM_EFI_MAIN], interval_str, 10);
	strlcat(time_str, interval_str, buf_len);

	strlcat(time_str, (CHAR8 *)",", buf_len);
	strlcat(time_str, (CHAR8 *)BOOT_STAGE_VERIFY_BOOT, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
	itoa(bt_stamp[TM_VERIFY_BOOT_DONE] - bt_stamp[TM_AVB_START], interval_str, 10);
	strlcat(time_str, interval_str, buf_len);

	strlcat(time_str, (CHAR8 *)",", buf_len);
#ifdef USE_TRUSTY
	strlcat(time_str, (CHAR8 *)BOOT_STAGE_VERIFY_TRUSTY, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
	itoa(bt_stamp[TM_VERIFY_TOS_DONE] - bt_stamp[TM_VERIFY_BOOT_DONE], interval_str, 10);
	strlcat(time_str, interval_str, buf_len);

	strlcat(time_str, (CHAR8 *)",", buf_len);
#endif
	strlcat(time_str, (CHAR8 *)BOOT_STAGE_START_KERNEL, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
#ifdef USE_TRUSTY
	itoa(bt_stamp[TM_JMP_KERNEL] - bt_stamp[TM_VERIFY_TOS_DONE], interval_str, 10);
#else
	itoa(bt_stamp[TM_JMP_KERNEL] - bt_stamp[TM_VERIFY_BOOT_DONE], interval_str, 10);
#endif

#else //#ifdef USE_AVB
	strlcat(time_str, (CHAR8 *)BOOT_STAGE_OSLOADER, buf_len);
	strlcat(time_str, (CHAR8 *)":", buf_len);
	itoa(bt_stamp[TM_JMP_KERNEL] - bt_stamp[TM_EFI_MAIN], interval_str, 10);
#endif

	strlcat(time_str, interval_str, buf_len);
}
