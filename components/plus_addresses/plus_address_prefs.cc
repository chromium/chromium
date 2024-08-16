// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_prefs.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"

namespace plus_addresses::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kPreallocatedAddressesVersion, 1);
  registry->RegisterListPref(kPreallocatedAddresses, base::Value::List());
  registry->RegisterIntegerPref(kPreallocatedAddressesNext, 0);
}

}  // namespace plus_addresses::prefs
