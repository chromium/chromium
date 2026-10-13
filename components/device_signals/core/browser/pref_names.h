// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_

class PrefRegistrySimple;

namespace device_signals {
namespace prefs {

// Whether or not admin requires managed users to share device signals when they
// sign in on an unmanaged device
inline constexpr char kUnmanagedDeviceSignalsConsentFlowEnabled[] =
    "device_signals.consent_collection_enabled";

// Whether the current user has given explicit consent to share device signals
// or not. User consent may not be required in certain management contexts.
inline constexpr char kDeviceSignalsConsentReceived[] =
    "device_signals.consent_received";

// Permanent version of `kDeviceSignalsConsentReceived`, where the consent is
// never reset once received. For now we need this for OIDC-managed profiles.
// On Android, the same key is also used as a per-account `SigninPrefs` entry
// (see //components/signin/public/base/signin_prefs_keys.h).
inline constexpr char kDeviceSignalsPermanentConsentReceived[] =
    "device_signals.permanent_consent_received";

}  // namespace prefs

// Registers user preferences related to Device Signal Sharing.
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_
