// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/pref_names.h"

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

namespace device_signals {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kUnmanagedDeviceSignalsConsentFlowEnabled, false);
#if !BUILDFLAG(IS_ANDROID)
  // On Android, consent is stored per account in `SigninPrefs` instead.
  registry->RegisterBooleanPref(prefs::kDeviceSignalsConsentReceived, false);
  registry->RegisterBooleanPref(prefs::kDeviceSignalsPermanentConsentReceived,
                                false);
#endif  // !BUILDFLAG(IS_ANDROID)
}
}  // namespace device_signals
