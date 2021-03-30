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
  TriggerContext::Options options;
  options.experiment_ids = "1,2,4";
  TriggerContext trigger_context = {std::make_unique<ScriptParameters>(),
                                    options};
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

  TriggerContext trigger_context;
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
  TriggerContext trigger_context = {
      std::make_unique<ScriptParameters>(
          std::map<std::string, std::string>{{"must_match", "matching_value"}}),
      {}};
  static_trigger_conditions_.Init(
      &mock_website_login_manager_, mock_is_first_time_user_callback_.Get(),
      GURL(kFakeUrl), &trigger_context, mock_callback_.Get());

  ScriptParameterMatchProto must_match;
  must_match.set_name("must_match");
  must_match.set_value_equals("matching_value");
  EXPECT_TRUE(static_trigger_conditions_.script_parameter_matches(must_match));

  must_match.set_value_equals("not_matching_value");
  EXPECT_FALSE(static_trigger_conditions_.script_parameter_matches(must_match));

  // More comprehensive test in |script_parameters_unittest|.
}

}  // namespace
}  // namespace autofill_assistant
