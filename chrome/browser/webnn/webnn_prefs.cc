// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webnn/webnn_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace webnn {

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kWinAppRuntimePackageFamilyName, {});
  registry->RegisterStringPref(prefs::kWinAppRuntimePackageMinVersion, {});
  registry->RegisterStringPref(prefs::kWinAppRuntimePackageDependencyId, {});
}

}  // namespace webnn
