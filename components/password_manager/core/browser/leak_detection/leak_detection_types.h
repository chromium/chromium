// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_TYPES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_TYPES_H_

#include "base/types/strong_alias.h"

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Needs to stay in sync with PasswordLeakDetectionError in enums.xml.
enum class LeakDetectionError {
  // The user isn't signed-in to Chrome.
  kNotSignIn = 0,
  // Error obtaining a token.
  kTokenRequestFailure = 1,
  // Error in hashing/encrypting for the request.
  kHashingFailure = 2,
  // Error obtaining a valid server response.
  kInvalidServerResponse = 3,
  // Error related to network connection.
  kNetworkError = 4,
  // The user ran out of quota.
  kQuotaLimit = 5,
  kMaxValue = kQuotaLimit,
};

using IsLeaked = base::StrongAlias<class IsLeakedTag, bool>;

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_TYPES_H_
