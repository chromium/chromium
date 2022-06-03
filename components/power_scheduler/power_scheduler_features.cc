// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_scheduler_features.h"

namespace power_scheduler {
namespace features {

// Enables the power scheduler. Defaults to throttling when idle or in no-op
// animations, if at least 250ms of CPU time were spent in the first 500ms after
// entering idle/no-op animation mode. Can be further configured via field trial
// parameters, see power_scheduler.h/cc for details.
const base::Feature kPowerScheduler{"PowerScheduler",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of Chrome's threads to use only LITTLE cores on big.LITTLE
// architectures.
const base::Feature kCpuAffinityRestrictToLittleCores{
    "CpuAffinityRestrictToLittleCores", base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of Chrome's threads to use only LITTLE cores on big.LITTLE
// architectures when the detected PowerMode is kIdle or kBackground.
const base::Feature kPowerSchedulerThrottleIdle{
    "PowerSchedulerThrottleIdle", base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of Chrome's threads to use only LITTLE cores on big.LITTLE
// architectures when the detected PowerMode is kIdle, kBackground, or
// kNopAnimation.
const base::Feature kPowerSchedulerThrottleIdleAndNopAnimation{
    "PowerSchedulerThrottleIdleAndNopAnimation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts WebView child processes to use only LITTLE cores on big.LITTLE
// architectures.
const base::Feature kWebViewCpuAffinityRestrictToLittleCores{
    "WebViewCpuAffinityRestrictToLittleCores",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Restricts all of WebView's out-of-process renderer threads to use only LITTLE
// cores on big.LITTLE architectures when the power mode is idle.
const base::Feature kWebViewPowerSchedulerThrottleIdle{
    "WebViewPowerSchedulerThrottleIdle", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace power_scheduler
