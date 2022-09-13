// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_driver.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "ios/web/public/navigation/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace sessions {

// Tests that PageState data is properly sanitized when post data is present.
TEST(IOSSerializedNavigationDriverTest, PickleSanitization) {
  IOSSerializedNavigationDriver* driver =
      IOSSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  EXPECT_EQ(test_data::kEncodedPageState, navigation.encoded_page_state());

  // Sanitization always clears the page state.
  std::string sanitized_page_state =
      driver->GetSanitizedPageStateForPickle(&navigation);
  EXPECT_EQ(std::string(), sanitized_page_state);
}

// Tests that the input data is left unsanitized when the referrer policy is
// Always.
TEST(IOSSerializedNavigationDriverTest, SanitizeWithReferrerPolicyAlways) {
  IOSSerializedNavigationDriver* driver =
      IOSSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      web::ReferrerPolicyAlways, &navigation);
  driver->Sanitize(&navigation);

  SerializedNavigationEntry reference_navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  EXPECT_EQ(reference_navigation.index(), navigation.index());
  EXPECT_EQ(reference_navigation.unique_id(), navigation.unique_id());
  EXPECT_EQ(reference_navigation.referrer_url(), navigation.referrer_url());
  EXPECT_EQ(web::ReferrerPolicyAlways, navigation.referrer_policy());
  EXPECT_EQ(reference_navigation.virtual_url(), navigation.virtual_url());
  EXPECT_EQ(reference_navigation.title(), navigation.title());
  EXPECT_EQ(reference_navigation.encoded_page_state(),
            navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), reference_navigation.transition_type()));
  EXPECT_EQ(reference_navigation.has_post_data(), navigation.has_post_data());
  EXPECT_EQ(reference_navigation.post_id(), navigation.post_id());
  EXPECT_EQ(reference_navigation.original_request_url(),
            navigation.original_request_url());
  EXPECT_EQ(reference_navigation.is_overriding_user_agent(),
            navigation.is_overriding_user_agent());
  EXPECT_EQ(reference_navigation.timestamp(), navigation.timestamp());
  EXPECT_EQ(reference_navigation.favicon_url(), navigation.favicon_url());
  EXPECT_EQ(reference_navigation.http_status_code(),
            navigation.http_status_code());
}

// Tests that the input data is properly sanitized when the referrer policy is
// Never.
TEST(IOSSerializedNavigationDriverTest, SanitizeWithReferrerPolicyNever) {
  IOSSerializedNavigationDriver* driver =
      IOSSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      web::ReferrerPolicyNever, &navigation);

  driver->Sanitize(&navigation);

  // Fields that should remain untouched.
  SerializedNavigationEntry reference_navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  EXPECT_EQ(reference_navigation.index(), navigation.index());
  EXPECT_EQ(reference_navigation.unique_id(), navigation.unique_id());
  EXPECT_EQ(reference_navigation.virtual_url(), navigation.virtual_url());
  EXPECT_EQ(reference_navigation.title(), navigation.title());
  EXPECT_EQ(reference_navigation.encoded_page_state(),
            navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), reference_navigation.transition_type()));
  EXPECT_EQ(reference_navigation.has_post_data(), navigation.has_post_data());
  EXPECT_EQ(reference_navigation.post_id(), navigation.post_id());
  EXPECT_EQ(reference_navigation.original_request_url(),
            navigation.original_request_url());
  EXPECT_EQ(reference_navigation.is_overriding_user_agent(),
            navigation.is_overriding_user_agent());
  EXPECT_EQ(reference_navigation.timestamp(), navigation.timestamp());
  EXPECT_EQ(reference_navigation.favicon_url(), navigation.favicon_url());
  EXPECT_EQ(reference_navigation.http_status_code(),
            navigation.http_status_code());

  // Fields that were sanitized.
  EXPECT_EQ(GURL(), navigation.referrer_url());
  EXPECT_EQ(web::ReferrerPolicyDefault, navigation.referrer_policy());
}

}  // namespace sessions
