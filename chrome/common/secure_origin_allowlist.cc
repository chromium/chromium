// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_allowlist.h"

#include <set>
#include <string>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace secure_origin_allowlist {

std::set<std::string> GetSchemesBypassingSecureContextCheck() {
  std::set<std::string> schemes;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  schemes.insert(extensions::kExtensionScheme);
#endif
  return schemes;
}

void RegisterPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterStringPref(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                                  /* default_value */ "");
}

}  // namespace secure_origin_allowlist
