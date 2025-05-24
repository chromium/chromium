// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include <string>

#include "base/base64.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_sync_data_fake.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using test::UrlStatus;

class SupervisedUserUtilsTest : public ::testing::Test {
 public:
  SupervisedUserUtilsTest() {
    RegisterProfilePrefs(pref_service_.registry());
    sync_data_fake_.Init();
    EnableParentalControls(pref_service_);
    filter_.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());
  }

  ~SupervisedUserUtilsTest() override = default;

  SupervisedUserURLFilter& filter() { return filter_; }
  test::SupervisedUserSyncDataFake<TestingPrefServiceSimple>& sync_data_fake() {
    return sync_data_fake_;
  }

 private:
  TestingPrefServiceSimple pref_service_;
  test::SupervisedUserSyncDataFake<TestingPrefServiceSimple> sync_data_fake_{
      pref_service_};
  SupervisedUserURLFilter filter_ =
      SupervisedUserURLFilter(pref_service_,
                              std::make_unique<FakeURLFilterDelegate>());
};

TEST_F(SupervisedUserUtilsTest, StripOnDefaultFilteringBehaviour) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::DEFAULT;
  UrlFormatter url_formatter(filter(), reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       StripOnManualFilteringBehaviourWithoutConflict) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::MANUAL;
  UrlFormatter url_formatter(filter(), reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       SkipStripOnManualFilteringBehaviourWithConflict) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::MANUAL;
  GURL full_url("http://www.example.com");

  // Add an conflicting entry in the blocklist.
  sync_data_fake().SetManualHosts({{full_url.host(), UrlStatus::kBlocked}});
  filter().UpdateManualHosts();

  UrlFormatter url_formatter(filter(), reason);

  EXPECT_EQ(full_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest, ParseParentAccessCallbackDecodingError) {
  std::string invalid_base64_message = "*INVALID*CHARS";
  ParentAccessCallbackParsedResult result =
      ParentAccessCallbackParsedResult::ParseParentAccessCallbackResult(
          invalid_base64_message);
  EXPECT_TRUE(result.GetError().has_value());
  EXPECT_EQ(ParentAccessWidgetError::kDecodingError, result.GetError().value());
}

TEST_F(SupervisedUserUtilsTest, ParseParentAccessCallbackParsingError) {
  std::string base64_non_pacp_message = base::Base64Encode("random_message");
  ParentAccessCallbackParsedResult result =
      ParentAccessCallbackParsedResult::ParseParentAccessCallbackResult(
          base64_non_pacp_message);
  EXPECT_TRUE(result.GetError().has_value());
  EXPECT_EQ(ParentAccessWidgetError::kParsingError, result.GetError().value());
}

TEST_F(SupervisedUserUtilsTest, ParseParentAccessCallbackApproveButtonResult) {
  // Constructed PACP approval message.
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnParentVerified*
      on_parent_verified = parent_access_callback.mutable_on_parent_verified();
  kids::platform::parentaccess::client::proto::ParentAccessToken* token =
      on_parent_verified->mutable_parent_access_token();
  token->set_token("TEST_TOKEN");
  kids::platform::parentaccess::client::proto::Timestamp* expire_time =
      token->mutable_expire_time();
  expire_time->set_seconds(123456);

  std::string base64_approve_pacp_message =
      base::Base64Encode(parent_access_callback.SerializeAsString());

  ParentAccessCallbackParsedResult response =
      ParentAccessCallbackParsedResult::ParseParentAccessCallbackResult(
          base64_approve_pacp_message);
  EXPECT_TRUE(response.GetCallback().has_value());
  EXPECT_EQ(kids::platform::parentaccess::client::proto::ParentAccessCallback::
                CallbackCase::kOnParentVerified,
            response.GetCallback().value().callback_case());
}

}  // namespace
}  // namespace supervised_user
