// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_

#include <string>

#include "components/sessions/core/serialized_navigation_entry.h"

namespace sessions {

// Set of test functions to manipulate a SerializedNavigationEntry.
class ContentTestHelper {
 public:
  // Only static methods.
  ContentTestHelper() = delete;
  ContentTestHelper(const ContentTestHelper&) = delete;
  ContentTestHelper& operator=(const ContentTestHelper&) = delete;

  // Creates a SerializedNavigationEntry with the given URL and title and some
  // common values for the other fields.
  static SerializedNavigationEntry CreateNavigation(
      const std::string& virtual_url,
      const std::string& title);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_
