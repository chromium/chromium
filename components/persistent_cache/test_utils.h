// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_
#define COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_

#include "testing/gmock/include/gmock/gmock.h"

// Matches the result of Find() operations to an `Entry` with specific contents.
MATCHER_P(HasContents, expected_span, "") {
  if (!arg) {
    *result_listener << "entry not found.";
    return false;
  }

  if (arg->GetContentSpan() == expected_span) {
    return true;
  }

  *result_listener << "contents do not match.";
  return false;
}

#endif  // COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_
