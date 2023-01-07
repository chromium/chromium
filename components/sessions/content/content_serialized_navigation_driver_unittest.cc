// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_driver.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

// Tests that PageState data is properly sanitized when post data is present.
TEST(ContentSerializedNavigationDriverTest, PickleSanitizationWithPostData) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  ASSERT_TRUE(navigation.has_post_data());

  // When post data is present, the page state should be sanitized.
  std::string sanitized_page_state =
      driver->GetSanitizedPageStateForPickle(&navigation);
  EXPECT_EQ(std::string(), sanitized_page_state);
}

// Tests that PageState data is left unsanitized when post data is absent.
TEST(ContentSerializedNavigationDriverTest, PickleSanitizationNoPostData) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetHasPostData(false, &navigation);
  ASSERT_FALSE(navigation.has_post_data());

  std::string sanitized_page_state =
      driver->GetSanitizedPageStateForPickle(&navigation);
  EXPECT_EQ(test_data::kEncodedPageState, sanitized_page_state);
}

}  // namespace sessions
