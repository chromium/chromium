// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_
#define COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace wallet::features {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kWalletApiPrivatePassesEnabled);
#else
BASE_DECLARE_FEATURE(kWalletApiPrivatePassesEnabled);
#endif

BASE_DECLARE_FEATURE_PARAM(std::string, kWalletSaveUrl);

BASE_DECLARE_FEATURE(kWalletApiPrivatePassesConsent);

BASE_DECLARE_FEATURE(kWalletablePassDetection);

BASE_DECLARE_FEATURE_PARAM(std::string,
                           kWalletablePassDetectionCountryAllowlist);

// This is a test only flag, and should be removed before starting the rollout.
BASE_DECLARE_FEATURE(kWalletablePassSave);

}  // namespace wallet::features

#endif  // COMPONENTS_WALLET_CORE_COMMON_WALLET_FEATURES_H_
