// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace site_token_provider::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      kSiteTokenProviderPref, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

}  // namespace site_token_provider::prefs
