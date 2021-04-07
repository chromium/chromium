// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include <map>
#include <memory>
#include <string>
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::WithArg;

const char kExampleUrl[] = "https://www.example.com";

class StarterTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kExampleUrl));

    starter_ = std::make_unique<Starter>(
        web_contents(), &fake_platform_delegate_, &ukm_recorder_);
  }

 protected:
  void AssertRecordedStartedState(Metrics::LiteScriptStarted state) {
    auto entries =
        ukm_recorder_.GetEntriesByName("AutofillAssistant.LiteScriptStarted");
    ASSERT_THAT(entries.size(), Eq(1u));
    ukm_recorder_.ExpectEntrySourceHasUrl(
        entries[0], web_contents()->GetLastCommittedURL());
    EXPECT_EQ(*ukm_recorder_.GetEntryMetric(entries[0], "LiteScriptStarted"),
              static_cast<int64_t>(state));
  }

  FakeStarterPlatformDelegate fake_platform_delegate_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<Starter> starter_;
  base::MockCallback<Starter::StarterResultCallback> mock_callback_;
};

TEST_F(StarterTest, FailWithoutInitialUrl) {
  std::map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", "abc"}};
  TriggerContext::Options options;
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _));
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());
  AssertRecordedStartedState(
      Metrics::LiteScriptStarted::LITE_SCRIPT_NO_INITIAL_URL);
}

TEST_F(StarterTest, FailWithoutMandatoryScriptParameter) {
  // ENABLED is missing from |params|.
  std::map<std::string, std::string> params = {
      {"START_IMMEDIATELY", "false"}, {"TRIGGER_SCRIPTS_BASE64", "abc"}};
  TriggerContext::Options options;
  options.initial_url = "https://www.example.com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _));
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());
  AssertRecordedStartedState(
      Metrics::LiteScriptStarted::LITE_SCRIPT_MANDATORY_PARAMETER_MISSING);
}

TEST_F(StarterTest, FailWhenFeatureDisabled) {
  std::map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", "abc"}};
  TriggerContext::Options options;
  options.initial_url = "https://www.example.com";
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndDisableFeature(
      features::kAutofillAssistantProactiveHelp);
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _));
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());
  AssertRecordedStartedState(
      Metrics::LiteScriptStarted::LITE_SCRIPT_FEATURE_DISABLED);
}

TEST_F(StarterTest, RegularStartupForReturningUsersSucceeds) {
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.is_first_time_user_ = false;
  fake_platform_delegate_.onboarding_accepted_ = true;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ true,
                                  GURL("https://www.example.com"), _))
      .WillOnce(WithArg<2>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(false));
      }));
  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());
  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
}

TEST_F(StarterTest, RegularStartupForFirstTimeUsersSucceeds) {
  fake_platform_delegate_.feature_module_installed_ = false;
  fake_platform_delegate_.is_first_time_user_ = true;
  fake_platform_delegate_.onboarding_accepted_ = false;
  fake_platform_delegate_.feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED;
  fake_platform_delegate_.show_onboarding_result_shown = true;
  fake_platform_delegate_.show_onboarding_result = OnboardingResult::ACCEPTED;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ true,
                                  GURL("https://www.example.com"), _))
      .WillOnce(WithArg<2>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(true));
      }));
  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());
  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called, Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called, Eq(1));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
}

TEST_F(StarterTest, RegularStartupFailsIfDfmInstallationFails) {
  fake_platform_delegate_.feature_module_installed_ = false;
  fake_platform_delegate_.onboarding_accepted_ = false;
  fake_platform_delegate_.feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_FAILED;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _));
  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());
  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called, Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
}

TEST_F(StarterTest, RegularStartupFailsIfOnboardingRejected) {
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.is_first_time_user_ = false;
  fake_platform_delegate_.onboarding_accepted_ = false;
  fake_platform_delegate_.show_onboarding_result_shown = true;
  fake_platform_delegate_.show_onboarding_result = OnboardingResult::REJECTED;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _));
  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
}

}  // namespace
}  // namespace autofill_assistant
