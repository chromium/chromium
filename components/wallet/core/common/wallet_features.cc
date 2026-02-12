// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/common/wallet_features.h"

namespace wallet::features {

// Controls whether the Wallet API is enabled.
BASE_FEATURE(kWalletApiPrivatePassesEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

// The backend URL to save the walletable pass.
BASE_FEATURE_PARAM(std::string,
                   kWalletSaveUrl,
                   &kWalletApiPrivatePassesEnabled,
                   "wallet_pass_save_url",
                   "");

// Controls whether to enable walletable pass detection on web pages.
BASE_FEATURE(kWalletablePassDetection, base::FEATURE_DISABLED_BY_DEFAULT);

// The allowlist is expected to consist of comma-separated uppercase
// two-digit country codes (see documentation of `GeoIpCountryCode`.)
BASE_FEATURE_PARAM(std::string,
                   kWalletablePassDetectionCountryAllowlist,
                   &kWalletablePassDetection,
                   "walletable_supported_country_allowlist",
                   "");

// Controls whether to enable saving walletable passes.
// This is a test only flag, and should be removed before starting the rollout.
BASE_FEATURE(kWalletablePassSave, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace wallet::features
