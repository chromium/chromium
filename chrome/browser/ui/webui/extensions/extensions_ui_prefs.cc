// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace extensions_ui_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDeveloperMode, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace extensions_ui_prefs
