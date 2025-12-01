// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_permission_utils.h"

#include <iterator>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/wallet/core/browser/data_models/country_type.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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

const absl::flat_hash_set<std::string>& GetAllowedCountries() {
  static const base::NoDestructor<absl::flat_hash_set<std::string>>
      allowed_countries([] {
        const std::string& allowlist =
            kWalletablePassDetectionCountryAllowlist.Get();
        std::vector<std::string> countries_vector = base::SplitString(
            allowlist, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
            base::SplitResult::SPLIT_WANT_NONEMPTY);
        return absl::flat_hash_set<std::string>(
            std::make_move_iterator(countries_vector.begin()),
            std::make_move_iterator(countries_vector.end()));
      }());
  return *allowed_countries;
}

// Checks whether `country_code` belongs to a country where Wallet is
// supported.
bool IsWalletSupportedCountry(const GeoIpCountryCode& country_code) {
  if (!base::FeatureList::IsEnabled(kWalletablePassDetection)) {
    // If the allowlist feature is disabled, no country is considered supported.
    return false;
  }
  if (country_code.value().empty()) {
    // An empty country code is considered supported if the feature is enabled.
    return true;
  }

  // Returns whether the allowlist contains `country_code` (case-insensitive).
  return base::Contains(GetAllowedCountries(), country_code.value());
}

}  // namespace

bool IsEligibleForWalletablePassDetection(
    const IdentityManager* identity_manager,
    const GeoIpCountryCode& country_code) {
  return base::FeatureList::IsEnabled(kWalletablePassDetection) &&
         GetAccountGaiaIdHash(identity_manager).has_value() &&
         IsWalletSupportedCountry(country_code);
}

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
