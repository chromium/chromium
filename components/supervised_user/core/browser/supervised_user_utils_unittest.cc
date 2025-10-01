// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include <string>

#include "base/base64.h"
#include "base/test/task_environment.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

class SupervisedUserUtilsTest : public ::testing::Test {
 protected:
  SupervisedUserUtilsTest() {
    EnableParentalControls(*supervised_user_test_environment_.pref_service());
  }
  ~SupervisedUserUtilsTest() override {
    supervised_user_test_environment_.Shutdown();
  }

  SupervisedUserTestEnvironment& supervised_user_test_environment() {
    return supervised_user_test_environment_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
};

TEST_F(SupervisedUserUtilsTest, StripOnDefaultFilteringBehaviour) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::DEFAULT;
  UrlFormatter url_formatter(*supervised_user_test_environment().url_filter(),
                             reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       StripOnManualFilteringBehaviourWithoutConflict) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::MANUAL;
  UrlFormatter url_formatter(*supervised_user_test_environment().url_filter(),
                             reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       SkipStripOnManualFilteringBehaviourWithConflict) {
  FilteringBehaviorReason reason = FilteringBehaviorReason::MANUAL;
  GURL full_url("http://www.example.com");

  // Add an conflicting entry in the blocklist.
  supervised_user_test_environment().SetManualFilterForHost(full_url.GetHost(),
                                                            false);

  UrlFormatter url_formatter(*supervised_user_test_environment().url_filter(),
                             reason);

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
