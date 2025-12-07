// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/prefs.h"

namespace enterprise_connectors {

const char kUserContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust_user.origins";

const char kBrowserContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust_browser.origins";

void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kUserContextAwareAccessSignalsAllowlistPref);
  registry->RegisterListPref(kBrowserContextAwareAccessSignalsAllowlistPref);
}

}  // namespace enterprise_connectors
