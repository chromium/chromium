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

#if BUILDFLAG(IS_ANDROID)
// Local State pref that stores the last time an attestation token was
// requested, either to pre-warm the attestation token provider or to sign a
// challenge response.
inline constexpr char kDeviceTrustAttestationTokenLastRequestTimePref[] =
    "enterprise_connectors.device_trust.attestation_token_last_request_time";
#endif  // BUILDFLAG(IS_ANDROID)

// Registers the device trust connectors profile preferences.
void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry);

// Registers the device trust connectors Local State preferences. Only Android
// has any.
void RegisterDeviceTrustConnectorLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_PREFS_H_
