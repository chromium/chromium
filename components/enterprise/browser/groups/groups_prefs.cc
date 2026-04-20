// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/groups/groups_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_groups {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kEnterpriseGroupsBrowserPref);
}

}  // namespace enterprise_groups
