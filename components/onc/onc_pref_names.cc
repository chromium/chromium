// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/onc/onc_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace onc {
namespace prefs {

// A pref to configure networks device-wide. Its value must be a list of
// NetworkConfigurations according to the OpenNetworkConfiguration
// specification.
// Currently, this pref is only used to store the policy. The user's
// configuration is still stored in Shill.
const char kDeviceOpenNetworkConfiguration[] = "device_onc";

// A pref to configure networks. Its value must be a list of
// NetworkConfigurations according to the OpenNetworkConfiguration
// specification.
// Currently, this pref is only used to store the policy. The user's
// configuration is still stored in Shill.
const char kOpenNetworkConfiguration[] = "onc";

}  // namespace prefs

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDeviceOpenNetworkConfiguration);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kOpenNetworkConfiguration);
}

}  // namespace onc
