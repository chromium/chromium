// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/prefs.h"

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/time/time.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace enterprise_connectors {

const char kUserContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust_user.origins";

const char kBrowserContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust_browser.origins";

void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kUserContextAwareAccessSignalsAllowlistPref);
  registry->RegisterListPref(kBrowserContextAwareAccessSignalsAllowlistPref);
}

void RegisterDeviceTrustConnectorLocalStatePrefs(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterTimePref(kDeviceTrustAttestationTokenLastRequestTimePref,
                             base::Time());
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace enterprise_connectors
