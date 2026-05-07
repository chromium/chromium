// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/prefs.h"

namespace enterprise_isolated_mode {

const char kEnterpriseIsolatedModeSettings[] = "enterprise.isolated_mode";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kEnterpriseIsolatedModeSettings, 0);
}

}  // namespace enterprise_isolated_mode
