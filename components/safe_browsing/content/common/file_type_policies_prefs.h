// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_PREFS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_PREFS_H_

class PrefRegistrySimple;

namespace safe_browsing::file_type {
namespace prefs {

extern const char kExemptDomainFileTypePairsFromFileTypeDownloadWarnings[];

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace safe_browsing::file_type

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_PREFS_H_