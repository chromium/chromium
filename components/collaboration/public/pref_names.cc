// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace collaboration::prefs {

const char kSharedTabGroupsManagedAccountSetting[] =
    "shared_tab_groups.managed_account_setting";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kSharedTabGroupsManagedAccountSetting,
                                0 /* Allowed */);
}

}  // namespace collaboration::prefs
