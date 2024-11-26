// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_

#include <string>

class GURL;
class PrefService;

namespace safe_browsing {

// A result of checking whether a given download's file type policies should be
// overridden and treated as "not dangerous".
enum class FileTypePoliciesOverrideResult {
  // Do not override; use the existing file type policies entry.
  kDoNotOverride,
  // Override and treat as "not dangerous".
  kOverrideAsNotDangerous,
};

// Determines whether an override should be applied to disregard any file type
// policies and treat the download as "not dangerous".
// If `url` is invalid, this always returns kDoNotOverride.
FileTypePoliciesOverrideResult ShouldOverrideFileTypePolicies(
    const std::string& extension,
    const GURL& url,
    const PrefService* pref_service);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_
