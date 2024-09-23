// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace device_signals {
namespace prefs {

// Whether or not admin requires managed users to share device signals when they
// sign in on an unmanaged device
const char kUnmanagedDeviceSignalsConsentFlowEnabled[] =
    "device_signals.consent_collection_enabled";

// Whether the current user has given explicit consent to share device signals
// or not. User consent may not be required in certain management contexts.
const char kDeviceSignalsConsentReceived[] = "device_signals.consent_received";

// Permanent version of `kDeviceSignalsConsentReceived`, where the consent is
// never reset once received. For now we need this for OIDC-managed profiles.
const char kDeviceSignalsPermanentConsentReceived[] =
    "device_signals.permanent_consent_received";

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kUnmanagedDeviceSignalsConsentFlowEnabled, false);
  registry->RegisterBooleanPref(prefs::kDeviceSignalsConsentReceived, false);
  registry->RegisterBooleanPref(prefs::kDeviceSignalsPermanentConsentReceived,
                                false);
}
}  // namespace device_signals
