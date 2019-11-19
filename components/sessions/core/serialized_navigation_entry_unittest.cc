// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <string>

#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_driver.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace sessions {
namespace {

// Create a default SerializedNavigationEntry.  All its fields should be
// initialized to their respective default values.
TEST(SerializedNavigationEntryTest, DefaultInitializer) {
  const SerializedNavigationEntry navigation;
  EXPECT_EQ(-1, navigation.index());
  EXPECT_EQ(0, navigation.unique_id());
  EXPECT_EQ(GURL(), navigation.referrer_url());
  EXPECT_EQ(
      SerializedNavigationDriver::Get()->GetDefaultReferrerPolicy(),
      navigation.referrer_policy());
  EXPECT_EQ(GURL(), navigation.virtual_url());
  EXPECT_TRUE(navigation.title().empty());
  EXPECT_EQ(std::string(), navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_TRUE(navigation.timestamp().is_null());
  EXPECT_FALSE(navigation.favicon_url().is_valid());
  EXPECT_EQ(0, navigation.http_status_code());
  EXPECT_EQ(0U, navigation.redirect_chain().size());
}

// Create a SerializedNavigationEntry, pickle it, then create another one by
// unpickling.  The new one should match the old one except for fields
// that aren't pickled, which should be set to default values.
TEST(SerializedNavigationEntryTest, Pickle) {
  const SerializedNavigationEntry old_navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();

  base::Pickle pickle;
  old_navigation.WriteToPickle(30000, &pickle);

  SerializedNavigationEntry new_navigation;
  base::PickleIterator pickle_iterator(pickle);
  EXPECT_TRUE(new_navigation.ReadFromPickle(&pickle_iterator));

  // Fields that are written to the pickle.
  EXPECT_EQ(test_data::kIndex, new_navigation.index());
  EXPECT_EQ(test_data::kReferrerURL, new_navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, new_navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, new_navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, new_navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      new_navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kHasPostData, new_navigation.has_post_data());
  EXPECT_EQ(test_data::kOriginalRequestURL,
            new_navigation.original_request_url());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            new_navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, new_navigation.timestamp());
  EXPECT_EQ(test_data::kHttpStatusCode, new_navigation.http_status_code());

  ASSERT_EQ(2U, new_navigation.extended_info_map().size());
  ASSERT_EQ(1U, new_navigation.extended_info_map().count(
                    test_data::kExtendedInfoKey1));
  EXPECT_EQ(
      test_data::kExtendedInfoValue1,
      new_navigation.extended_info_map().at(test_data::kExtendedInfoKey1));
  ASSERT_EQ(1U, new_navigation.extended_info_map().count(
                    test_data::kExtendedInfoKey2));
  EXPECT_EQ(
      test_data::kExtendedInfoValue2,
      new_navigation.extended_info_map().at(test_data::kExtendedInfoKey2));

  EXPECT_EQ(test_data::kTaskId, new_navigation.task_id());
  EXPECT_EQ(test_data::kParentTaskId, new_navigation.parent_task_id());
  EXPECT_EQ(test_data::kRootTaskId, new_navigation.root_task_id());
  EXPECT_EQ(test_data::kChildrenTaskIds, new_navigation.children_task_ids());

  // Fields that are not written to the pickle.
  EXPECT_EQ(0, new_navigation.unique_id());
  EXPECT_EQ(std::string(), new_navigation.encoded_page_state());
  EXPECT_EQ(-1, new_navigation.post_id());
  EXPECT_EQ(0U, new_navigation.redirect_chain().size());
}

}  // namespace
}  // namespace sessions
