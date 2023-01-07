// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {

// Controls the ChromeOS /proc/sys/vm/min_filelist_kb swap tunable, if the
// feature is enabled it will use the value (in MB) from the feature param.
BASE_DECLARE_FEATURE(kCrOSTuneMinFilelist);
extern const base::FeatureParam<int> kCrOSTuneMinFilelistMb;

// Controls the ChromeOS /sys/kernel/mm/chromeos-low_mem/ram_vs_swap_weight
// tunable. The number is a zero or positive number which represents how well
// zram based swap is compressed in physical ram.
BASE_DECLARE_FEATURE(kCrOSTuneRamVsSwapWeight);
extern const base::FeatureParam<int> kCrOSRamVsSwapWeight;

// Controls the ChromeOS /proc/sys/vm/extra_free_kbytes tunable. The number is a
// zero or positive number which represents how much additional memory the
// kernel will keep around. Raising this number has the affect of causing
// swapping earlier.
BASE_DECLARE_FEATURE(kCrOSTuneExtraFree);
extern const base::FeatureParam<int> kCrOSExtraFreeMb;

// This feature and params control the zram writeback behavior.
COMPONENT_EXPORT(ASH_MEMORY) BASE_DECLARE_FEATURE(kCrOSEnableZramWriteback);

// Controls the period in which the controller will check to see if we can write
// back. It does not guarantee a writeback will actually happen.
extern const base::FeatureParam<int> kCrOSWritebackPeriodicTimeSec;

// This is the minimum amount of time allowed required writebacks, regardless of
// system state.
extern const base::FeatureParam<int> kCrOSZramWritebackWritebackBackoffTimeSec;

// This is the percentage of free space to use on the stateful partition for the
// backing device.
extern const base::FeatureParam<int> kCrOSZramWritebackDevSizePctFree;

// This is the absolute minimum number of pages we would consider writing back.
// That is, the policy will never return a value smaller than this.
extern const base::FeatureParam<int> kCrOSWritebackMinPages;

// This is the maximum number of pages the policy will allow to be written back
// in a single period.
extern const base::FeatureParam<int> kCrOSWritebackMaxPages;

// If enabled we will write back pages as "Huge Idle." Huge idle pages are pages
// which have been idle for the idle time AND are huge (meaning incompressible).
extern const base::FeatureParam<bool> kCrOSWritebackHugeIdle;

// If enabled we will writeback idle pages, which are pages which have been in
// zram for an amount of time greater than the idle time.
extern const base::FeatureParam<bool> kCrOSWritebackIdle;

// If enabled we will writeback huge pages, these are pages which are
// incompressible with no requirement on the amount of time they have been
// inzram.
extern const base::FeatureParam<bool> kCrOSWritebackHuge;

// This is the hard lower bound on zram idle and huge idle writeback modes idle
// time. This will create a lower limit on how long a page MUST be in zram
// before it can be written back.
extern const base::FeatureParam<int> kCrOSWritebackIdleMinTimeSec;

// This is the hard upper bound on the idle time. That is, the policy will never
// return a value larger than this for the idle time.
extern const base::FeatureParam<int> kCrOSWritebackIdleMaxTimeSec;

struct ZramWritebackParams {
  base::TimeDelta periodic_time;
  base::TimeDelta backoff_time;
  uint64_t min_pages;
  uint64_t max_pages;
  bool writeback_huge_idle;
  bool writeback_idle;
  bool writeback_huge;
  base::TimeDelta idle_min_time;
  base::TimeDelta idle_max_time;
  static const ZramWritebackParams Get();
};

// Configure swap will configure any swap related experiments that this user may
// be opted into.
COMPONENT_EXPORT(ASH_MEMORY) void ConfigureSwap();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_
