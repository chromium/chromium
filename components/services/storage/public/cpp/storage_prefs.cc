// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/storage_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace storage {

// Boolean policy to force WebSQL to be enabled.
const char kWebSQLAccess[] = "policy.web_sql_access";

// Boolean policy to force WebSQL in non-secure contexts to be enabled.
const char kWebSQLNonSecureContextEnabled[] =
    "policy.web_sql_non_secure_context_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWebSQLAccess, false);
  registry->RegisterBooleanPref(kWebSQLNonSecureContextEnabled, false);
}

}  // namespace storage
