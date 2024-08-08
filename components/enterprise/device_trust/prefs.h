// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_PREFS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_PREFS_H_

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

// Pref that maps to the "UserContextAwareAccessSignalsAllowlist" policy.
extern const char kUserContextAwareAccessSignalsAllowlistPref[];

// Pref that maps to the "BrowserContextAwareAccessSignalsAllowlist" policy.
extern const char kBrowserContextAwareAccessSignalsAllowlistPref[];

// Registers the device trust connectors profile preferences.
void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_PREFS_H_
