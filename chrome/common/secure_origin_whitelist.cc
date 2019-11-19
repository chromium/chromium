// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include <set>
#include <string>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/common/constants.h"

namespace secure_origin_whitelist {

std::set<std::string> GetSchemesBypassingSecureContextCheck() {
  std::set<std::string> schemes;
  schemes.insert(extensions::kExtensionScheme);
  return schemes;
}

void RegisterPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterStringPref(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                                  /* default_value */ "");
}

}  // namespace secure_origin_whitelist
