// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_builder.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using IOSSerializedNavigationBuilderTest = web::WebTest;

namespace sessions {

namespace {
// Creates a NavigationItem from the test_data constants in
// serialized_navigation_entry_test_helper.h.
std::unique_ptr<web::NavigationItem> MakeNavigationItemForTest() {
  std::unique_ptr<web::NavigationItem> navigation_item(
      web::NavigationItem::Create());
  navigation_item->SetReferrer(web::Referrer(
      test_data::kReferrerURL,
      static_cast<web::ReferrerPolicy>(test_data::kReferrerPolicy)));
  navigation_item->SetURL(test_data::kVirtualURL);
  navigation_item->SetTitle(test_data::kTitle);
  navigation_item->SetTransitionType(test_data::kTransitionType);
  navigation_item->SetTimestamp(test_data::kTimestamp);
  navigation_item->GetFavicon().valid = true;
  navigation_item->GetFavicon().url = test_data::kFaviconURL;
  return navigation_item;
}

}  // namespace

// Create a SerializedNavigationEntry from a NavigationItem.  All its fields
// should match the NavigationItem's.
TEST_F(IOSSerializedNavigationBuilderTest, FromNavigationItem) {
  const std::unique_ptr<web::NavigationItem> navigation_item(
      MakeNavigationItemForTest());

  const SerializedNavigationEntry& navigation =
      IOSSerializedNavigationBuilder::FromNavigationItem(
          test_data::kIndex, *navigation_item);

  EXPECT_EQ(test_data::kIndex, navigation.index());

  EXPECT_EQ(navigation_item->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());

  // The following fields should be left at their default values.
  SerializedNavigationEntry default_navigation;
  EXPECT_EQ(default_navigation.encoded_page_state(),
            navigation.encoded_page_state());
  EXPECT_EQ(default_navigation.has_post_data(), navigation.has_post_data());
  EXPECT_EQ(default_navigation.post_id(), navigation.post_id());
  EXPECT_EQ(default_navigation.original_request_url(),
            navigation.original_request_url());
  EXPECT_EQ(default_navigation.is_overriding_user_agent(),
            navigation.is_overriding_user_agent());
  EXPECT_EQ(default_navigation.http_status_code(),
            navigation.http_status_code());
  ASSERT_EQ(0U, navigation.redirect_chain().size());
}

// Create a NavigationItem, then create another one by converting to
// a SerializedNavigationEntry and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST_F(IOSSerializedNavigationBuilderTest, ToNavigationItem) {
  const std::unique_ptr<web::NavigationItem> old_navigation_item(
      MakeNavigationItemForTest());

  const SerializedNavigationEntry& navigation =
      IOSSerializedNavigationBuilder::FromNavigationItem(
          test_data::kIndex, *old_navigation_item);

  const std::unique_ptr<web::NavigationItem> new_navigation_item(
      IOSSerializedNavigationBuilder::ToNavigationItem(&navigation));

  EXPECT_EQ(old_navigation_item->GetURL(),
            new_navigation_item->GetURL());
  EXPECT_EQ(old_navigation_item->GetReferrer().url,
            new_navigation_item->GetReferrer().url);
  EXPECT_EQ(old_navigation_item->GetReferrer().policy,
            new_navigation_item->GetReferrer().policy);
  EXPECT_EQ(old_navigation_item->GetVirtualURL(),
            new_navigation_item->GetVirtualURL());
  EXPECT_EQ(old_navigation_item->GetTitle(),
            new_navigation_item->GetTitle());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
                  new_navigation_item->GetTransitionType(),
                  ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(old_navigation_item->GetTimestamp(),
            new_navigation_item->GetTimestamp());
}

}  // namespace sessions
