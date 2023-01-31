// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/storage_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace storage {

// Boolean policy to force WebSQL to be enabled.
const char kWebSQLAccess[] = "policy.web_sql_access";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWebSQLAccess, false);
}

}  // namespace storage
