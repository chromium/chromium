// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PERMISSION_UTILS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PERMISSION_UTILS_H_

#include "components/wallet/core/browser/data_models/country_type.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace wallet {

// Returns the walletable pass detection opt-in status for the profile and
// account. Opt-in status is a profile pref, but keyed by (hashed) GAIA id.
// In particular, it is always `false` for users without a signed-in primary
// account.
[[nodiscard]] bool GetWalletablePassDetectionOptInStatus(
    const PrefService* pref_service,
    const signin::IdentityManager* identity_manager);

// Sets the walletable pass detection opt-in status for the profile and account.
void SetWalletablePassDetectionOptInStatus(
    PrefService* pref_service,
    const signin::IdentityManager* identity_manager,
    bool opt_in_status);

// Checks whether the user is eligible for walletable pass detection.
[[nodiscard]] bool IsEligibleForWalletablePassDetection(
    const signin::IdentityManager* identity_manager,
    const GeoIpCountryCode& country_code);

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PERMISSION_UTILS_H_
