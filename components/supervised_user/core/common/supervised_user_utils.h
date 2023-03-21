// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_

#include <string>

class GURL;

namespace supervised_user {

// Reason for applying the website filtering parental control.
enum class FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  DENYLIST = 2,
  MANUAL = 3,
  ALLOWLIST = 4,
  NOT_SIGNED_IN = 5,
};

// Converts FilteringBehaviorReason enum to string format.
std::string FilteringBehaviorReasonToString(FilteringBehaviorReason reason);

// Strips user-specific tokens in a URL to generalize it.
GURL NormalizeUrl(const GURL& url);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
