// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_

#include <string>

class GURL;
class PrefService;

namespace safe_browsing {

bool IsInNotDangerousOverrideList(const std::string& extension,
                                  const GURL& url,
                                  const PrefService* pref_service);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_POLICY_UTIL_H_