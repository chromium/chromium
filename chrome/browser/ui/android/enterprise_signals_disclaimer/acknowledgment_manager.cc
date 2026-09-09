// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/acknowledgment_manager.h"

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "google_apis/gaia/gaia_id.h"

namespace enterprise_signals_disclaimer {

using signin::GaiaIdHash;

const char kAcknowledgmentSetPrefPath[] =
    "enterprise_signals.acknowledgment_set";

void SetAccountAckedSignalsDisclaimer(PrefService& local_state,
                                      const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());

  const std::string gaia_id_hash = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();

  if (local_state.GetList(kAcknowledgmentSetPrefPath).contains(gaia_id_hash)) {
    // The account has already acknowledged the disclaimer.
    return;
  }

  ScopedListPrefUpdate update(local_state, kAcknowledgmentSetPrefPath);
  if (!update->contains(gaia_id_hash)) {
    update->Append(gaia_id_hash);
  }
}

bool HasAccountAckedSignalsDisclaimer(const PrefService& local_state,
                                      const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());

  const std::string gaia_id_hash = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
  const base::ListValue& ack_set =
      local_state.GetList(kAcknowledgmentSetPrefPath);
  return ack_set.contains(gaia_id_hash);
}

void RemoveUnknownAccounts(PrefService& local_state,
                           const base::span<const GaiaId> accounts_on_device) {
  const base::ListValue& current_ack_set =
      local_state.GetList(kAcknowledgmentSetPrefPath);

  const auto hashes = base::MakeFlatSet<std::string>(
      accounts_on_device, std::less<>(), [](const GaiaId& account) {
        CHECK(!account.empty());
        return GaiaIdHash::FromGaiaId(account).ToBase64();
      });

  const auto is_account_missing_on_device = [&hashes](const base::Value& val) {
    return !hashes.contains(val.GetString());
  };

  // ScopedListPrefUpdate will trigger a pref updated notification even if no
  // changes are made. Because of this we still need to compute hashes to be
  // removed to avoid unnecessary pref updates.
  const bool should_update =
      std::ranges::any_of(current_ack_set, is_account_missing_on_device);
  if (should_update) {
    ScopedListPrefUpdate update(&local_state, kAcknowledgmentSetPrefPath);
    update->EraseIf(is_account_missing_on_device);
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kAcknowledgmentSetPrefPath, {});
}

}  // namespace enterprise_signals_disclaimer
