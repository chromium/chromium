// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace safe_browsing::file_type {

namespace prefs {

// A list of file type and domains pairs that overrides the heuristics checks
// performed when downloading the specified file types from the specified
// domains.
const char kExemptDomainFileTypePairsFromFileTypeDownloadWarnings[] =
    "downloads.exempt_domain_filetype_pair_from_file_type_downloads_warnings";

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings);
}

}  // namespace safe_browsing::file_type