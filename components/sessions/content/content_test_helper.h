// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_

#include <string>

#include "base/macros.h"
#include "components/sessions/core/serialized_navigation_entry.h"

namespace sessions {

// Set of test functions to manipulate a SerializedNavigationEntry.
class ContentTestHelper {
 public:
  // Creates a SerializedNavigationEntry with the given URL and title and some
  // common values for the other fields.
  static SerializedNavigationEntry CreateNavigation(
      const std::string& virtual_url,
      const std::string& title);

 private:
  // Only static methods.
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentTestHelper);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_TEST_HELPER_H_
