// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_
#define CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Controls the ChromeOS /proc/sys/vm/min_filelist_kb swap tunable, if the
// feature is enabled it will use the value (in MB) from the feature param.
extern const base::Feature kCrOSTuneMinFilelist;
extern const base::FeatureParam<int> kCrOSTuneMinFilelistMb;

// Controls the ChromeOS /sys/kernel/mm/chromeos-low_mem/ram_vs_swap_weight
// tunable. The number is a zero or positive number which represents how well
// zram based swap is compressed in physical ram.
extern const base::Feature kCrOSTuneRamVsSwapWeight;
extern const base::FeatureParam<int> kCrOSRamVsSwapWeight;

// Controls the ChromeOS /proc/sys/vm/extra_free_kbytes tunable. The number is a
// zero or positive number which represents how much additional memory the
// kernel will keep around. Raising this number has the affect of causing
// swapping earlier.
extern const base::Feature kCrOSTuneExtraFree;
extern const base::FeatureParam<int> kCrOSExtraFreeMb;

// Configure swap will configure any swap related experiments that this user may
// be opted into.
CHROMEOS_EXPORT void ConfigureSwap();

}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_
