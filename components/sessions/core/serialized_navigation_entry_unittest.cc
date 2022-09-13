// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <string>

#include "base/pickle.h"
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
  EXPECT_EQ(old_navigation.index(), new_navigation.index());
  EXPECT_EQ(old_navigation.referrer_url(), new_navigation.referrer_url());
  EXPECT_EQ(old_navigation.referrer_policy(), new_navigation.referrer_policy());
  EXPECT_EQ(old_navigation.virtual_url(), new_navigation.virtual_url());
  EXPECT_EQ(old_navigation.title(), new_navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      new_navigation.transition_type(), old_navigation.transition_type()));
  EXPECT_EQ(old_navigation.has_post_data(), new_navigation.has_post_data());
  EXPECT_EQ(old_navigation.original_request_url(),
            new_navigation.original_request_url());
  EXPECT_EQ(old_navigation.is_overriding_user_agent(),
            new_navigation.is_overriding_user_agent());
  EXPECT_EQ(old_navigation.timestamp(), new_navigation.timestamp());
  EXPECT_EQ(old_navigation.http_status_code(),
            new_navigation.http_status_code());
  EXPECT_EQ(old_navigation.extended_info_map(),
            new_navigation.extended_info_map());
  EXPECT_EQ(old_navigation.task_id(), new_navigation.task_id());
  EXPECT_EQ(old_navigation.parent_task_id(), new_navigation.parent_task_id());
  EXPECT_EQ(old_navigation.root_task_id(), new_navigation.root_task_id());

  // Fields that are not written to the pickle.
  EXPECT_EQ(0, new_navigation.unique_id());
  EXPECT_EQ(std::string(), new_navigation.encoded_page_state());
  EXPECT_EQ(-1, new_navigation.post_id());
  EXPECT_EQ(0U, new_navigation.redirect_chain().size());
}

}  // namespace
}  // namespace sessions
