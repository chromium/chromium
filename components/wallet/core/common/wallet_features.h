// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_
#define COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace wallet {

BASE_DECLARE_FEATURE(kWalletablePassDetection);

extern const base::FeatureParam<std::string>
    kWalletablePassDetectionCountryAllowlist;

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_
