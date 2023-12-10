// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry_test_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sessions {

namespace test_data {

const int kIndex = 3;
const int kUniqueID = 50;
const int kReferrerPolicy = 0;
const std::u16string kTitle = u"title";
const std::string kEncodedPageState = "page state";
const ui::PageTransition kTransitionType =
    ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_AUTO_SUBFRAME |
        ui::PAGE_TRANSITION_HOME_PAGE |
        ui::PAGE_TRANSITION_CLIENT_REDIRECT);
const bool kHasPostData = true;
const int64_t kPostID = 100;
const bool kIsOverridingUserAgent = true;
const base::Time kTimestamp = base::Time::UnixEpoch() + base::Milliseconds(100);
const int kHttpStatusCode = 404;
const SerializedNavigationEntry::PasswordState kPasswordState =
    SerializedNavigationEntry::HAS_PASSWORD_FIELD;
const std::string kExtendedInfoKey1 = "key 1";
const std::string kExtendedInfoKey2 = "key 2";
const std::string kExtendedInfoValue1 = "value 1";
const std::string kExtendedInfoValue2 = "value 2";
const int64_t kTaskId = 2;
const int64_t kParentTaskId = 1;
const int64_t kRootTaskId = 0;

}  // namespace test_data

// static
void SerializedNavigationEntryTestHelper::ExpectNavigationEquals(
    const SerializedNavigationEntry& expected,
    const SerializedNavigationEntry& actual) {
  EXPECT_EQ(expected.referrer_url_, actual.referrer_url_);
  EXPECT_EQ(expected.referrer_policy_, actual.referrer_policy_);
  EXPECT_EQ(expected.virtual_url_, actual.virtual_url_);
  EXPECT_EQ(expected.title_, actual.title_);
  EXPECT_EQ(expected.encoded_page_state_, actual.encoded_page_state_);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      actual.transition_type_, expected.transition_type_));
  EXPECT_EQ(expected.has_post_data_, actual.has_post_data_);
  EXPECT_EQ(expected.original_request_url_, actual.original_request_url_);
  EXPECT_EQ(expected.is_overriding_user_agent_,
            actual.is_overriding_user_agent_);
}

// static
SerializedNavigationEntry
SerializedNavigationEntryTestHelper::CreateNavigationForTest() {
  SerializedNavigationEntry navigation;
  navigation.index_ = test_data::kIndex;
  navigation.unique_id_ = test_data::kUniqueID;
  navigation.referrer_url_ = GURL("http://www.referrer.com");
  navigation.referrer_policy_ = test_data::kReferrerPolicy;
  navigation.virtual_url_ = GURL("http://www.virtual-url.com");
  navigation.title_ = test_data::kTitle;
  navigation.encoded_page_state_ = test_data::kEncodedPageState;
  navigation.transition_type_ = test_data::kTransitionType;
  navigation.has_post_data_ = test_data::kHasPostData;
  navigation.post_id_ = test_data::kPostID;
  navigation.original_request_url_ = GURL("http://www.original-request.com");
  navigation.is_overriding_user_agent_ = test_data::kIsOverridingUserAgent;
  navigation.timestamp_ = test_data::kTimestamp;
  navigation.favicon_url_ = GURL("http://virtual-url.com/favicon.ico");
  navigation.http_status_code_ = test_data::kHttpStatusCode;
  navigation.password_state_ = test_data::kPasswordState;

  navigation.extended_info_map_[test_data::kExtendedInfoKey1] =
      test_data::kExtendedInfoValue1;
  navigation.extended_info_map_[test_data::kExtendedInfoKey2] =
      test_data::kExtendedInfoValue2;

  navigation.redirect_chain_.emplace_back("http://go/redirect0");
  navigation.redirect_chain_.emplace_back("http://go/redirect1");
  navigation.redirect_chain_.push_back(navigation.virtual_url_);
  navigation.task_id_ = test_data::kTaskId;
  navigation.parent_task_id_ = test_data::kParentTaskId;
  navigation.root_task_id_ = test_data::kRootTaskId;
  return navigation;
}

// static
void SerializedNavigationEntryTestHelper::SetReferrerPolicy(
    int policy,
    SerializedNavigationEntry* navigation) {
  navigation->referrer_policy_ = policy;
}

// static
void SerializedNavigationEntryTestHelper::SetVirtualURL(
    const GURL& virtual_url,
    SerializedNavigationEntry* navigation) {
  navigation->virtual_url_ = virtual_url;
}

// static
void SerializedNavigationEntryTestHelper::SetEncodedPageState(
    const std::string& encoded_page_state,
    SerializedNavigationEntry* navigation) {
  navigation->encoded_page_state_ = encoded_page_state;
}

// static
void SerializedNavigationEntryTestHelper::SetTransitionType(
    ui::PageTransition transition_type,
    SerializedNavigationEntry* navigation) {
  navigation->transition_type_ = transition_type;
}

// static
void SerializedNavigationEntryTestHelper::SetHasPostData(
    bool has_post_data,
    SerializedNavigationEntry* navigation) {
  navigation->has_post_data_ = has_post_data;
}

// static
void SerializedNavigationEntryTestHelper::SetOriginalRequestURL(
    const GURL& original_request_url,
    SerializedNavigationEntry* navigation) {
  navigation->original_request_url_ = original_request_url;
}

// static
void SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(
    bool is_overriding_user_agent,
    SerializedNavigationEntry* navigation) {
  navigation->is_overriding_user_agent_ = is_overriding_user_agent;
}

// static
void SerializedNavigationEntryTestHelper::SetTimestamp(
    base::Time timestamp,
    SerializedNavigationEntry* navigation) {
  navigation->timestamp_ = timestamp;
}

}  // namespace sessions
