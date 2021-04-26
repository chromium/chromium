// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include <map>
#include <memory>
#include <string>
#include "base/base64url.h"
#include "base/memory/checked_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_trigger_script_ui_delegate.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::WithArg;
using ::testing::WithArgs;

const char kExampleUrl[] = "https://www.example.com";

class StarterTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kExampleUrl));
    auto mock_trigger_script_ui_delegate =
        std::make_unique<NiceMock<MockTriggerScriptUiDelegate>>();
    mock_trigger_script_ui_delegate_ = mock_trigger_script_ui_delegate.get();
    fake_platform_delegate_.trigger_script_ui_delegate_ =
        std::move(mock_trigger_script_ui_delegate);

    ON_CALL(*mock_trigger_script_ui_delegate_, Attach)
        .WillByDefault(WithArg<0>(
            [&](TriggerScriptCoordinator* trigger_script_coordinator) {
              trigger_script_coordinator_ = trigger_script_coordinator;
            }));
    ON_CALL(*mock_trigger_script_ui_delegate_, Detach).WillByDefault([&]() {
      trigger_script_coordinator_ = nullptr;
    });
    auto mock_trigger_script_service_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_trigger_script_service_request_sender_ =
        mock_trigger_script_service_request_sender.get();
    fake_platform_delegate_.trigger_script_request_sender_for_test_ =
        std::move(mock_trigger_script_service_request_sender);
    fake_platform_delegate_.website_login_manager_ =
        &mock_website_login_manager_;
    ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl)
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>()));

    starter_ = std::make_unique<Starter>(
        web_contents(), &fake_platform_delegate_, &ukm_recorder_,
        mock_runtime_manager_.GetWeakPtr());
  }

  void TearDown() override {
    // Note: it is important to reset the starter explicitly here to ensure that
    // destructors are called on the right thread, as required by devtools.
    starter_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetupPlatformDelegateForFirstTimeUser() {
    fake_platform_delegate_.feature_module_installed_ = false;
    fake_platform_delegate_.is_first_time_user_ = true;
    fake_platform_delegate_.onboarding_accepted_ = false;
    fake_platform_delegate_.feature_module_installation_result_ = Metrics::
        FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED;
    fake_platform_delegate_.show_onboarding_result_shown_ = true;
    fake_platform_delegate_.show_onboarding_result_ =
        OnboardingResult::ACCEPTED;
  }

  void SetupPlatformDelegateForReturningUser() {
    fake_platform_delegate_.feature_module_installed_ = true;
    fake_platform_delegate_.is_first_time_user_ = false;
    fake_platform_delegate_.onboarding_accepted_ = true;
  }

  // Returns a base64-encoded GetTriggerScriptsResponseProto containing a single
  // trigger script without any trigger conditions. As such, it will be shown
  // immediately upon startup.
  std::string CreateBase64TriggerScriptResponseForTest() {
    GetTriggerScriptsResponseProto get_trigger_scripts_response;
    get_trigger_scripts_response.add_trigger_scripts();
    std::string serialized_get_trigger_scripts_response;
    get_trigger_scripts_response.SerializeToString(
        &serialized_get_trigger_scripts_response);
    std::string base64_get_trigger_scripts_response;
    base::Base64UrlEncode(serialized_get_trigger_scripts_response,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &base64_get_trigger_scripts_response);
    return base64_get_trigger_scripts_response;
  }

  // Returns true if a specific |state| was recorded for |entry_name| and
  // |metric_name|.
  bool RecordedUkmMetric(base::StringPiece entry_name,
                         base::StringPiece metric_name,
                         int64_t expected_state) {
    auto entries = ukm_recorder_.GetEntriesByName(entry_name);
    if (entries.size() != 1) {
      return false;
    }
    ukm_recorder_.ExpectEntrySourceHasUrl(
        entries[0], web_contents()->GetLastCommittedURL());
    const int64_t* actual_state =
        ukm_recorder_.GetEntryMetric(entries[0], metric_name);
    return actual_state != nullptr && *actual_state == expected_state;
  }

  // Returns whether anything was recorded for |entry_name|.
  bool RecordedUkmMetric(base::StringPiece entry_name) {
    return !ukm_recorder_.GetEntriesByName(entry_name).empty();
  }

  bool UkmLiteScriptStarted(Metrics::LiteScriptStarted state) {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptStarted",
                             "LiteScriptStarted", static_cast<int64_t>(state));
  }

  bool UkmLiteScriptFinished(Metrics::LiteScriptFinishedState state) {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptFinished",
                             "LiteScriptFinished", static_cast<int64_t>(state));
  }

  bool UkmLiteScriptOnboarding(Metrics::LiteScriptOnboarding result) {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptOnboarding",
                             "LiteScriptOnboarding",
                             static_cast<int64_t>(result));
  }

  bool UkmLiteScriptStarted() {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptStarted");
  }

  bool UkmLiteScriptFinished() {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptFinished");
  }

  bool UkmLiteScriptOnboarding() {
    return RecordedUkmMetric("AutofillAssistant.LiteScriptOnboarding");
  }

  // Simulates a redirect-navigation to |redirect_url|, followed by a regular
  // navigation to |url|.
  void SimulateRedirectToUrl(const GURL& url, const GURL& redirect_url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(url), web_contents()->GetMainFrame());
    simulator->Start();
    simulator->Redirect(redirect_url);
    simulator->Commit();
    simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(url), web_contents()->GetMainFrame());
    simulator->Start();
    simulator->Commit();
  }

  CheckedPtr<NiceMock<MockTriggerScriptUiDelegate>>
      mock_trigger_script_ui_delegate_ = nullptr;
  CheckedPtr<NiceMock<MockServiceRequestSender>>
      mock_trigger_script_service_request_sender_ = nullptr;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
  // Only set while a trigger script is running.
  CheckedPtr<TriggerScriptCoordinator> trigger_script_coordinator_ = nullptr;
  FakeStarterPlatformDelegate fake_platform_delegate_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  MockRuntimeManager mock_runtime_manager_;
  std::unique_ptr<Starter> starter_;
  base::HistogramTester histogram_tester_;
  base::MockCallback<Starter::StarterResultCallback> mock_callback_;
};

TEST_F(StarterTest, RegularScriptFailsWithoutInitialUrl) {
  std::map<std::string, std::string> params = {{"ENABLED", "true"},
                                               {"START_IMMEDIATELY", "true"}};
  TriggerContext::Options options;
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());

  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, TriggerScriptFailsWithoutInitialUrl) {
  std::map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", "abc"}};
  TriggerContext::Options options;
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_NO_INITIAL_URL));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, FailWithoutMandatoryScriptParameter) {
  // ENABLED is missing from |params|.
  std::map<std::string, std::string> params = {
      {"START_IMMEDIATELY", "false"}, {"TRIGGER_SCRIPTS_BASE64", "abc"}};
  TriggerContext::Options options;
  options.initial_url = "https://www.example.com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_MANDATORY_PARAMETER_MISSING));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
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
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(params), options),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_FEATURE_DISABLED));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, RegularStartupForReturningUsersSucceeds) {
  SetupPlatformDelegateForReturningUser();
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ true,
                                  GURL("https://www.example.com"), _, _))
      .WillOnce(WithArg<2>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(false));
      }));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(0));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_ACCEPTED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_NOT_SHOWN, 1u);
}

TEST_F(StarterTest, RegularStartupForFirstTimeUsersSucceeds) {
  SetupPlatformDelegateForFirstTimeUser();
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ true,
                                  GURL("https://www.example.com"), _, _))
      .WillOnce(WithArg<2>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(true));
      }));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED,
      1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_ACCEPTED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_SHOWN, 1u);
}

TEST_F(StarterTest, RegularStartupFailsIfDfmInstallationFails) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_FAILED;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_FAILED,
      1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, RegularStartupFailsIfOnboardingRejected) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::REJECTED;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"},
      {"INTENT", "SHOPPING_ASSISTED_CHECKOUT"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_CANCELLED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_SHOWN, 1u);
}

TEST_F(StarterTest, RpcTriggerScriptFailsIfMsbbIsDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.msbb_enabled_ = false;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options()),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, RpcTriggerScriptFailsIfProactiveHelpIsDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.proactive_help_enabled_ = false;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options()),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, RpcTriggerScriptSucceeds) {
  // A trigger script without trigger conditions will be shown immediately.
  GetTriggerScriptsResponseProto get_trigger_scripts_response;
  get_trigger_scripts_response.add_trigger_scripts()
      ->mutable_user_interface()
      ->set_status_message("trigger script");
  std::string serialized_get_trigger_scripts_response;
  get_trigger_scripts_response.SerializeToString(
      &serialized_get_trigger_scripts_response);

  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  options.onboarding_shown = false;
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  EXPECT_CALL(*mock_trigger_script_service_request_sender_,
              OnSendRequest(
                  GURL("https://automate-pa.googleapis.com/v1/triggers"), _, _))
      .WillOnce(
          WithArgs<1, 2>([&](const std::string& request_body,
                             ServiceRequestSender::ResponseCallback& callback) {
            // Note that trigger scripts should be fetched for the
            // ORIGINAL_DEEPLINK, not for the |initial_url|.
            GetTriggerScriptsRequestProto request;
            ASSERT_TRUE(request.ParseFromString(request_body));
            EXPECT_THAT(request.url(), Eq(GURL("https://www.example.com")));
            std::move(callback).Run(net::HTTP_OK,
                                    serialized_get_trigger_scripts_response);
          }));
  EXPECT_CALL(
      mock_callback_,
      Run(/* start_regular_script = */ true, GURL("https://www.example.com"),
          Pointee(Property(&TriggerContext::GetOnboardingShown, true)),
          Optional(get_trigger_scripts_response.trigger_scripts(0))));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_FIRST_TIME_USER));
  EXPECT_TRUE(UkmLiteScriptFinished(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED));
  EXPECT_TRUE(UkmLiteScriptOnboarding(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, Base64TriggerScriptFailsForInvalidBase64) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", "#invalid_hashtag"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options()),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_RETURNING_USER));
  EXPECT_TRUE(UkmLiteScriptFinished(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_BASE64_DECODING_ERROR));
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, Base64TriggerScriptFailsIfProactiveHelpIsDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.proactive_help_enabled_ = false;
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", CreateBase64TriggerScriptResponseForTest()},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));

  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options()),
                  mock_callback_.Get());

  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED));
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, Base64TriggerScriptSucceeds) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Base64 trigger scripts should not require MSBB to be enabled.
  fake_platform_delegate_.msbb_enabled_ = false;
  // No need to inject a mock request sender for base64 trigger scripts, we can
  // use the real one.
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", CreateBase64TriggerScriptResponseForTest()},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  options.onboarding_shown = false;
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  EXPECT_CALL(
      mock_callback_,
      Run(/* start_regular_script = */ true, GURL("https://www.example.com"),
          Pointee(Property(&TriggerContext::GetOnboardingShown, true)),
          testing::Ne(base::nullopt)));

  starter_->Start(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_callback_.Get());

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_FIRST_TIME_USER));
  EXPECT_TRUE(UkmLiteScriptFinished(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED));
  EXPECT_TRUE(UkmLiteScriptOnboarding(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, CancelPendingTriggerScriptWhenTransitioningFromCctToTab) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", CreateBase64TriggerScriptResponseForTest()},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};

  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript);
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options{}),
                  mock_callback_.Get());

  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));
  fake_platform_delegate_.is_custom_tab_ = false;
  starter_->CheckSettings();
}

TEST_F(StarterTest, RegularStartupFailsIfNavigationDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ =
      base::DoNothing::Once<base::OnceCallback<void(bool, OnboardingResult)>>();

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options{}),
                  mock_callback_.Get());

  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.different.com"));

  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_NO_ANSWER, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_SHOWN, 1u);
}

TEST_F(StarterTest, TriggerScriptStartupFailsIfNavigationDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ =
      base::DoNothing::Once<base::OnceCallback<void(bool, OnboardingResult)>>();

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"TRIGGER_SCRIPTS_BASE64", CreateBase64TriggerScriptResponseForTest()},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options{}),
                  mock_callback_.Get());

  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));
  EXPECT_TRUE(UkmLiteScriptStarted(
      Metrics::LiteScriptStarted::LITE_SCRIPT_FIRST_TIME_USER));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.different.com"));
  // TODO(b/185476714): Fix lite script metrics to record for ORIGINAL_DEEPLINK
  // instead of the current URL.
  EXPECT_TRUE(UkmLiteScriptFinished(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE));
  EXPECT_TRUE(UkmLiteScriptOnboarding(
      Metrics::LiteScriptOnboarding::
          LITE_SCRIPT_ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

TEST_F(StarterTest, RegularStartupAllowsCertainNavigationsDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ =
      base::DoNothing::Once<base::OnceCallback<void(bool, OnboardingResult)>>();

  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options{}),
                  mock_callback_.Get());

  // Expect that the onboarding is not interrupted by a redirect to the
  // ORIGINAL_DEEPLINK.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  SimulateRedirectToUrl(GURL("https://www.example.com"),
                        GURL("http://redirect.example.com"));
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);

  // Redirecting to a different URL will cancel the onboarding.
  EXPECT_CALL(mock_callback_, Run(/* start_regular_script = */ false, _, _, _));
  SimulateRedirectToUrl(GURL("https://www.different.com"),
                        GURL("http://redirect.example.com"));

  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_NO_ANSWER, 1u);
  histogram_tester_.ExpectBucketCount("Android.AutofillAssistant.OnBoarding",
                                      Metrics::OnBoarding::OB_SHOWN, 1u);
}

TEST_F(StarterTest, RegularStartupIgnoresLastCommittedUrl) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ =
      base::DoNothing::Once<base::OnceCallback<void(bool, OnboardingResult)>>();

  // Note: the starter does not actually care about the last committed URL at
  // the time of startup. All that matters is that it has received the startup
  // intent, and that there is a valid ORIGINAL_DEEPLINK to expect.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.ignored.com"));
  std::map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(mock_callback_, Run).Times(0);
  starter_->Start(std::make_unique<TriggerContext>(
                      std::make_unique<ScriptParameters>(script_parameters),
                      TriggerContext::Options{}),
                  mock_callback_.Get());

  EXPECT_FALSE(UkmLiteScriptStarted());
  EXPECT_FALSE(UkmLiteScriptFinished());
  EXPECT_FALSE(UkmLiteScriptOnboarding());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  histogram_tester_.ExpectTotalCount("Android.AutofillAssistant.OnBoarding",
                                     0u);
}

}  // namespace
}  // namespace autofill_assistant
