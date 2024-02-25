// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

namespace certificate_transparency {
namespace prefs {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kCTExcludedHosts);
  registry->RegisterListPref(prefs::kCTExcludedSPKIs);
  registry->RegisterListPref(prefs::kCTExcludedLegacySPKIs);
}

const char kCTExcludedHosts[] = "certificate_transparency.excluded_hosts";

const char kCTExcludedSPKIs[] = "certificate_transparency.excluded_spkis";

const char kCTExcludedLegacySPKIs[] =
    "certificate_transparency.excluded_legacy_spkis";

}  // namespace prefs
}  // namespace certificate_transparency
