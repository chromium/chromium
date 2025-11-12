// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_permission_utils.h"

#include <optional>

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/wallet/core/common/wallet_prefs.h"

namespace wallet {
namespace {

using ::signin::GaiaIdHash;
using ::signin::IdentityManager;

// Returns the `GaiaIdHash` for the signed in account if there is one or
// `std::nullopt` otherwise.
[[nodiscard]] std::optional<GaiaIdHash> GetAccountGaiaIdHash(
    const IdentityManager* identity_manager) {
  if (!identity_manager) {
    return std::nullopt;
  }
  GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  if (gaia_id.empty()) {
    return std::nullopt;
  }
  return GaiaIdHash::FromGaiaId(gaia_id);
}

}  // namespace

bool GetWalletablePassDetectionOptInStatus(
    const PrefService* pref_service,
    const IdentityManager* identity_manager) {
  if (!pref_service) {
    return false;
  }

  // Check the account-dependent opt-in setting.
  const std::optional<GaiaIdHash> signed_in_hash =
      GetAccountGaiaIdHash(identity_manager);
  if (!signed_in_hash) {
    return false;
  }
  const base::Value* value = syncer::GetAccountKeyedPrefValue(
      pref_service, prefs::kWalletablePassDetectionOptInStatus,
      *signed_in_hash);
  return value && value->GetIfBool().value_or(false);
}

void SetWalletablePassDetectionOptInStatus(
    PrefService* prefs,
    const IdentityManager* identity_manager,
    bool opt_in_status) {
  if (!prefs) {
    return;
  }
  const std::optional<GaiaIdHash> signed_in_hash =
      GetAccountGaiaIdHash(identity_manager);
  if (signed_in_hash) {
    syncer::SetAccountKeyedPrefValue(
        prefs, prefs::kWalletablePassDetectionOptInStatus, *signed_in_hash,
        base::Value(opt_in_status));
  }
}

}  // namespace wallet
