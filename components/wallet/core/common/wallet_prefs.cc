// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/common/wallet_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace wallet::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kWalletablePassDetectionOptInStatus);
}

}  // namespace wallet::prefs
