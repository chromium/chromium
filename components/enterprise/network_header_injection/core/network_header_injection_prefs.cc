// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_custom_headers {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kHttpHeaderInjection);
}

}  // namespace enterprise_custom_headers
