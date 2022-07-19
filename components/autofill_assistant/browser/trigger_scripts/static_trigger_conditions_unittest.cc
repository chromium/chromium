// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/trigger_context.h"

#include "base/containers/flat_map.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;

namespace {

const char kFakeUrl[] = "https://www.example.com";

class StaticTriggerConditionsTest : public testing::Test {
 public:
  StaticTriggerConditionsTest() {
    fake_platform_delegate_.website_login_manager_ =
        &mock_website_login_manager_;
  }

  ~StaticTriggerConditionsTest() override = default;

 protected:
  base::MockCallback<base::OnceCallback<void(void)>> mock_callback_;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
  FakeStarterPlatformDelegate fake_platform_delegate_;
};

TEST_F(StaticTriggerConditionsTest, Update) {
  TriggerContext::Options options;
  options.experiment_ids = "1,2,4";
  TriggerContext trigger_context = {std::make_unique<ScriptParameters>(),
                                    options};
  StaticTriggerConditions static_trigger_conditions = {
      fake_platform_delegate_.GetWeakPtr(), &trigger_context, GURL(kFakeUrl)};
  fake_platform_delegate_.is_first_time_user_ = true;
  EXPECT_CALL(mock_website_login_manager_, GetLoginsForUrl(GURL(kFakeUrl), _))
      .WillOnce(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), "fake_username")}));
  EXPECT_CALL(mock_callback_, Run).Times(1);
  static_trigger_conditions.Update(mock_callback_.Get());

  EXPECT_TRUE(static_trigger_conditions.is_first_time_user());
  EXPECT_TRUE(static_trigger_conditions.has_stored_login_credentials());
  EXPECT_TRUE(static_trigger_conditions.is_in_experiment(1));
  EXPECT_TRUE(static_trigger_conditions.is_in_experiment(2));
  EXPECT_FALSE(static_trigger_conditions.is_in_experiment(3));
  EXPECT_TRUE(static_trigger_conditions.is_in_experiment(4));
}

TEST_F(StaticTriggerConditionsTest, HasResults) {
  TriggerContext trigger_context;
  StaticTriggerConditions static_trigger_conditions = {
      fake_platform_delegate_.GetWeakPtr(), &trigger_context, GURL(kFakeUrl)};
  EXPECT_FALSE(static_trigger_conditions.has_results());

  EXPECT_CALL(mock_website_login_manager_, GetLoginsForUrl(GURL(kFakeUrl), _))
      .WillOnce(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), "fake_username")}));
  EXPECT_CALL(mock_callback_, Run).Times(1);
  static_trigger_conditions.Update(mock_callback_.Get());
  EXPECT_TRUE(static_trigger_conditions.has_results());
}

TEST_F(StaticTriggerConditionsTest, ScriptParameterMatches) {
  TriggerContext trigger_context = {
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{
              {"must_match", "matching_value"}}),
      {}};
  StaticTriggerConditions static_trigger_conditions = {
      fake_platform_delegate_.GetWeakPtr(), &trigger_context, GURL(kFakeUrl)};

  ScriptParameterMatchProto must_match;
  must_match.set_name("must_match");
  must_match.set_value_equals("matching_value");
  EXPECT_TRUE(static_trigger_conditions.script_parameter_matches(must_match));

  must_match.set_value_equals("not_matching_value");
  EXPECT_FALSE(static_trigger_conditions.script_parameter_matches(must_match));

  // More comprehensive test in |script_parameters_unittest|.
}

TEST_F(StaticTriggerConditionsTest, CachesFirstTimeUserFlag) {
  TriggerContext trigger_context = {std::make_unique<ScriptParameters>(),
                                    TriggerContext::Options{}};
  StaticTriggerConditions static_trigger_conditions = {
      fake_platform_delegate_.GetWeakPtr(), &trigger_context, GURL(kFakeUrl)};
  fake_platform_delegate_.is_first_time_user_ = true;
  EXPECT_CALL(mock_website_login_manager_, GetLoginsForUrl)
      .WillRepeatedly(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));
  static_trigger_conditions.Update(mock_callback_.Get());
  EXPECT_TRUE(static_trigger_conditions.is_first_time_user());

  fake_platform_delegate_.is_first_time_user_ = false;
  EXPECT_TRUE(static_trigger_conditions.is_first_time_user());

  static_trigger_conditions.Update(mock_callback_.Get());
  EXPECT_FALSE(static_trigger_conditions.is_first_time_user());
}

}  // namespace
}  // namespace autofill_assistant
