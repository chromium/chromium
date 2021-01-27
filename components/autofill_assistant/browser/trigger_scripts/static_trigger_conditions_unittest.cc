// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

#include "components/autofill_assistant/browser/mock_website_login_manager.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kFakeUrl[] = "https://www.example.com";

class StaticTriggerConditionsTest : public testing::Test {
 public:
  StaticTriggerConditionsTest() = default;
  ~StaticTriggerConditionsTest() override = default;

 protected:
  StaticTriggerConditions static_trigger_conditions_;
  base::MockCallback<base::RepeatingCallback<bool(void)>>
      mock_is_first_time_user_callback_;
  base::MockCallback<base::OnceCallback<void(void)>> mock_callback_;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
};

TEST_F(StaticTriggerConditionsTest, Init) {
  TriggerContextImpl trigger_context(/* params = */ {}, /* exp = */ "1,2,4");
  EXPECT_CALL(mock_is_first_time_user_callback_, Run).WillOnce(Return(true));
  EXPECT_CALL(mock_website_login_manager_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .WillOnce(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), "fake_username")}));
  EXPECT_CALL(mock_callback_, Run).Times(1);
  static_trigger_conditions_.Init(
      &mock_website_login_manager_, mock_is_first_time_user_callback_.Get(),
      GURL(kFakeUrl), &trigger_context, mock_callback_.Get());

  EXPECT_TRUE(static_trigger_conditions_.is_first_time_user());
  EXPECT_TRUE(static_trigger_conditions_.has_stored_login_credentials());
  EXPECT_TRUE(static_trigger_conditions_.is_in_experiment(1));
  EXPECT_TRUE(static_trigger_conditions_.is_in_experiment(2));
  EXPECT_FALSE(static_trigger_conditions_.is_in_experiment(3));
  EXPECT_TRUE(static_trigger_conditions_.is_in_experiment(4));
}

TEST_F(StaticTriggerConditionsTest, SetIsFirstTimeUser) {
  EXPECT_TRUE(static_trigger_conditions_.is_first_time_user());
  static_trigger_conditions_.set_is_first_time_user(false);
  EXPECT_FALSE(static_trigger_conditions_.is_first_time_user());
}

TEST_F(StaticTriggerConditionsTest, HasResults) {
  EXPECT_FALSE(static_trigger_conditions_.has_results());

  TriggerContextImpl trigger_context(/* params = */ {}, /* exp = */ "1,2,4");
  EXPECT_CALL(mock_is_first_time_user_callback_, Run).WillOnce(Return(true));
  EXPECT_CALL(mock_website_login_manager_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .WillOnce(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), "fake_username")}));
  EXPECT_CALL(mock_callback_, Run).Times(1);
  static_trigger_conditions_.Init(
      &mock_website_login_manager_, mock_is_first_time_user_callback_.Get(),
      GURL(kFakeUrl), &trigger_context, mock_callback_.Get());
  EXPECT_TRUE(static_trigger_conditions_.has_results());
}

TEST_F(StaticTriggerConditionsTest, ScriptParameterMatches) {
  TriggerContextImpl trigger_context({{"must_exist_and_exists", "exists"},
                                      {"must_not_exist_and_exists", "exists"},
                                      {"must_match", "matching_value"},
                                      {"must_match_empty", ""}},
                                     /* exp = */ "");
  static_trigger_conditions_.Init(
      &mock_website_login_manager_, mock_is_first_time_user_callback_.Get(),
      GURL(kFakeUrl), &trigger_context, mock_callback_.Get());

  ScriptParameterMatchProto must_exist;
  must_exist.set_name("must_exist_and_exists");
  must_exist.set_exists(true);
  EXPECT_TRUE(static_trigger_conditions_.script_parameter_matches(must_exist));

  must_exist.set_name("must_exist_and_doesnt_exist");
  EXPECT_FALSE(static_trigger_conditions_.script_parameter_matches(must_exist));

  ScriptParameterMatchProto must_not_exist;
  must_not_exist.set_name("must_not_exist_and_doesnt_exist");
  must_not_exist.set_exists(false);
  EXPECT_TRUE(
      static_trigger_conditions_.script_parameter_matches(must_not_exist));

  must_not_exist.set_name("must_not_exist_and_exists");
  EXPECT_FALSE(
      static_trigger_conditions_.script_parameter_matches(must_not_exist));

  ScriptParameterMatchProto must_match;
  must_match.set_name("must_match");
  must_match.set_value_equals("matching_value");
  EXPECT_TRUE(static_trigger_conditions_.script_parameter_matches(must_match));

  must_match.set_value_equals("not_matching_value");
  EXPECT_FALSE(static_trigger_conditions_.script_parameter_matches(must_match));

  must_match.set_value_equals("");
  EXPECT_FALSE(static_trigger_conditions_.script_parameter_matches(must_match));

  must_match.set_name("must_match_doesnt_exist");
  EXPECT_FALSE(static_trigger_conditions_.script_parameter_matches(must_match));

  ScriptParameterMatchProto must_match_empty;
  must_match_empty.set_name("must_match_empty");
  must_match_empty.set_value_equals("");
  EXPECT_TRUE(
      static_trigger_conditions_.script_parameter_matches(must_match_empty));

  must_match_empty.set_value_equals("not_empty");
  EXPECT_FALSE(
      static_trigger_conditions_.script_parameter_matches(must_match_empty));
}

}  // namespace
}  // namespace autofill_assistant
