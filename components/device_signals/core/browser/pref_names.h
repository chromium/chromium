// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_

class PrefRegistrySimple;

namespace device_signals {
namespace prefs {
extern const char kUnmanagedDeviceSignalsConsentFlowEnabled[];
extern const char kDeviceSignalsConsentReceived[];
extern const char kDeviceSignalsPermanentConsentReceived[];
}  // namespace prefs

// Registers user preferences related to Device Signal Sharing.
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_PREF_NAMES_H_
