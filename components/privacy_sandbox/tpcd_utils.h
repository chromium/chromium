// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TPCD_UTILS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TPCD_UTILS_H_

namespace tpcd::experiment::utils {

// A normal workflow can be one of the following:
// 1. `kUnknownEligibility` -> `kIneligible`
// 2. `kUnknownEligibility` -> `kEligible` (-> `kOnboarded`)
enum class ExperimentState {
  kUnknownEligibility = 0,
  kIneligible = 1,
  kEligible = 2,
  // This is only used when "disable_3p_cookies" feature param is true.
  kOnboarded = 3,
  kMaxValue = kOnboarded,
};

}  // namespace tpcd::experiment::utils

#endif  // COMPONENTS_PRIVACY_SANDBOX_TPCD_UTILS_H_
