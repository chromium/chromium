// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/acknowledgment_manager.h"

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

void SetAccountAcknowledgedSignalsDisclaimer(PrefService* local_state,
                                             const GaiaId& gaia_id) {
  CHECK(local_state);
  CHECK(!gaia_id.empty());

  const std::string gaia_id_hash = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();

  if (local_state->GetList(kAcknowledgmentSetPrefPath).contains(gaia_id_hash)) {
    // The account has already acknowledged the disclaimer.
    return;
  }

  ScopedListPrefUpdate update(local_state, kAcknowledgmentSetPrefPath);
  if (!update->contains(gaia_id_hash)) {
    update->Append(gaia_id_hash);
  }
}

bool HasAccountAcknowledgedSignalsDisclaimer(const PrefService* local_state,
                                             const GaiaId& gaia_id) {
  CHECK(local_state);
  CHECK(!gaia_id.empty());

  const std::string gaia_id_hash = GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
  const base::ListValue& ack_set =
      local_state->GetList(kAcknowledgmentSetPrefPath);
  return ack_set.contains(gaia_id_hash);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kAcknowledgmentSetPrefPath, {});
}

}  // namespace enterprise_signals_disclaimer
