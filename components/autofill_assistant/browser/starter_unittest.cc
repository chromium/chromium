// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_trigger_script_ui_delegate.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;
using ::testing::WithArgs;

const char kExampleDeeplink[] = "https://www.example.com";

class StarterTest : public testing::Test {
 public:
  StarterTest() {
    // Needs to be set to enable launched configs that are geo-gated by country.
    fake_platform_delegate_.fake_common_dependencies_->permanent_country_code_ =
        "us";
  }

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    SimulateNavigateToUrl(GURL(kExampleDeeplink));
    PrepareTriggerScriptUiDelegate();
    PrepareTriggerScriptRequestSender();
    fake_platform_delegate_.website_login_manager_ =
        &mock_website_login_manager_;

    ON_CALL(mock_website_login_manager_, GetLoginsForUrl)
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>()));

    starter_ = std::make_unique<Starter>(
        web_contents(), fake_platform_delegate_.GetWeakPtr(), &ukm_recorder_,
        mock_runtime_manager_.GetWeakPtr(),
        task_environment()->GetMockTickClock());
  }

  void TearDown() override {
    // We need to clear the static cache to avoid cross-talk between tests.
    GetFailedTriggerFetchesCacheForTest()->Clear();

    // Note: it is important to reset the starter explicitly here to ensure that
    // destructors are called on the right thread, as required by devtools.
    starter_.reset();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  base::HashingLRUCache<std::string, base::TimeTicks>*
  GetFailedTriggerFetchesCacheForTest() {
    return starter_->cached_failed_trigger_script_fetches_;
  }

  base::HashingLRUCache<std::string, base::TimeTicks>*
  GetUserDenylistedCacheForTest() const {
    return &starter_->user_denylisted_domains_;
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

  // Returns a serialized GetTriggerScriptsResponseProto containing a single
  // trigger script without any trigger conditions. As such, it will be shown
  // immediately upon startup.
  std::string CreateTriggerScriptResponseForTest(
      TriggerScriptProto::TriggerUIType trigger_ui_type =
          TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE) {
    GetTriggerScriptsResponseProto get_trigger_scripts_response;
    get_trigger_scripts_response.add_trigger_scripts()->set_trigger_ui_type(
        trigger_ui_type);
    std::string serialized_get_trigger_scripts_response;
    get_trigger_scripts_response.SerializeToString(
        &serialized_get_trigger_scripts_response);
    return serialized_get_trigger_scripts_response;
  }

  // Simulates a navigation from the last committed URL to urls[size-1] along
  // the intermediate redirect-hops in |urls|.
  void SimulateRedirectToUrl(const std::vector<GURL>& urls) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            web_contents()->GetLastCommittedURL(),
            web_contents()->GetPrimaryMainFrame());
    simulator->Start();
    for (const auto& url : urls) {
      simulator->Redirect(url);
    }
    simulator->Commit();
    navigation_ids_.emplace_back(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  void SimulateNavigateToUrl(const GURL& url) {
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
    navigation_ids_.emplace_back(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  // Each request sender is only good for one trigger script. This call will
  // create a new mock and prepare it to be used in the next call.
  void PrepareTriggerScriptRequestSender() {
    auto mock_trigger_script_service_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_trigger_script_service_request_sender_ =
        mock_trigger_script_service_request_sender.get();
    fake_platform_delegate_.trigger_script_request_sender_for_test_ =
        std::move(mock_trigger_script_service_request_sender);
  }

  // Configures mock_trigger_script_service_request_sender_ with a test response
  // and ensures that it is called once.
  void SetupRpcTriggerScriptResponseOnce(
      const std::string& url,
      const std::string& trigger_script_response) {
    EXPECT_CALL(
        *mock_trigger_script_service_request_sender_,
        OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                      _, RpcType::GET_TRIGGER_SCRIPTS))
        .WillOnce(WithArgs<1, 2>(
            [&, url, trigger_script_response](
                const std::string& request_body,
                ServiceRequestSender::ResponseCallback& callback) {
              GetTriggerScriptsRequestProto request;
              ASSERT_TRUE(request.ParseFromString(request_body));
              EXPECT_THAT(request.url(), Eq(GURL(url)));
              EXPECT_FALSE(request.client_context().is_in_chrome_triggered());
              std::move(callback).Run(net::HTTP_OK, trigger_script_response,
                                      ServiceRequestSender::ResponseInfo{});
            }));
  }

  void SetupRpcTriggerScriptResponseOnce(const std::string& url) {
    SetupRpcTriggerScriptResponseOnce(url,
                                      CreateTriggerScriptResponseForTest());
  }

  // Each trigger script UI delegate is only good for one trigger script and
  // must be prepared anew if a test needs to show more than one trigger script.
  void PrepareTriggerScriptUiDelegate() {
    auto mock_trigger_script_ui_delegate =
        std::make_unique<NiceMock<MockTriggerScriptUiDelegate>>();
    mock_trigger_script_ui_delegate_ = mock_trigger_script_ui_delegate.get();
    fake_platform_delegate_.trigger_script_ui_delegate_ =
        std::move(mock_trigger_script_ui_delegate);
    fake_platform_delegate_.start_regular_script_callback_ =
        mock_start_regular_script_callback_.Get();

    ON_CALL(*mock_trigger_script_ui_delegate_, Attach)
        .WillByDefault(WithArg<0>(
            [&](TriggerScriptCoordinator* trigger_script_coordinator) {
              trigger_script_coordinator_ = trigger_script_coordinator;
            }));
    ON_CALL(*mock_trigger_script_ui_delegate_, Detach).WillByDefault([&]() {
      trigger_script_coordinator_ = nullptr;
    });
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<NiceMock<MockTriggerScriptUiDelegate>>
      mock_trigger_script_ui_delegate_ = nullptr;
  raw_ptr<NiceMock<MockServiceRequestSender>>
      mock_trigger_script_service_request_sender_ = nullptr;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
  // Only set while a trigger script is running.
  raw_ptr<TriggerScriptCoordinator> trigger_script_coordinator_ = nullptr;

  signin::IdentityTestEnvironment identity_test_environment;
  signin::IdentityManager* identity_manager_ =
      identity_test_environment.identity_manager();
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(
          std::make_unique<FakeCommonDependencies>(identity_manager_));

  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  MockRuntimeManager mock_runtime_manager_;
  std::unique_ptr<Starter> starter_;
  base::HistogramTester histogram_tester_;
  base::MockCallback<base::OnceCallback<void(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script)>>
      mock_start_regular_script_callback_;
  std::vector<ukm::SourceId> navigation_ids_;
};

TEST_F(StarterTest, RegularScriptFailsWithoutInitialUrl) {
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"}, {"START_IMMEDIATELY", "true"}};
  TriggerContext::Options options;
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(params), options));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::FAILED_NO_INITIAL_URL,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, TriggerScriptFailsWithoutInitialUrl) {
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"}};
  TriggerContext::Options options;
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(params), options));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptStarted::NO_INITIAL_URL}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::FAILED_NO_INITIAL_URL,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, FailWithoutMandatoryScriptParameter) {
  // ENABLED is missing from |params|.
  base::flat_map<std::string, std::string> params = {
      {"START_IMMEDIATELY", "false"}, {"REQUEST_TRIGGER_SCRIPT", "true"}};
  TriggerContext::Options options;
  options.initial_url = kExampleDeeplink;
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(params), options));

  EXPECT_THAT(
      GetUkmTriggerScriptStarted(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptStarted::MANDATORY_PARAMETER_MISSING}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::
                         FAILED_MANDATORY_PARAMETER_MISSING,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, FailWhenFeatureDisabled) {
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"INTENT", "SHOPPING"},
      {"EXPERIMENT_IDS", "fake_experiment"},
      {"CALLER", "7"},
      {"SOURCE", "1"}};
  TriggerContext::Options options;
  options.initial_url = kExampleDeeplink;
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndDisableFeature(
      features::kAutofillAssistantProactiveHelp);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(params), options));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptStarted::FEATURE_DISABLED}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(
      GetUkmStartRequest(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::AutofillAssistantStarted::FAILED_FEATURE_DISABLED,
             AutofillAssistantIntent::SHOPPING,
             Metrics::AutofillAssistantCaller::IN_CHROME,
             Metrics::AutofillAssistantSource::ORGANIC,
             Metrics::AutofillAssistantExperiment::UNKNOWN_EXPERIMENT}}})));
}

TEST_F(StarterTest, RegularStartupForReturningUsersSucceeds) {
  SetupPlatformDelegateForReturningUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_,
              Run(GURL(kExampleDeeplink), _, _))
      .WillOnce(WithArg<1>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(false));
      }));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(0));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(
      GetUkmRegularScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0], {Metrics::Onboarding::OB_NOT_SHOWN}}})));
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::OK_IMMEDIATE_START,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, RegularStartupForFirstTimeUsersSucceeds) {
  SetupPlatformDelegateForFirstTimeUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_,
              Run(GURL(kExampleDeeplink), _, _))
      .WillOnce(WithArg<1>([](std::unique_ptr<TriggerContext> trigger_context) {
        EXPECT_THAT(trigger_context->GetOnboardingShown(), Eq(true));
      }));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(true));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED,
      1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0], {Metrics::Onboarding::OB_ACCEPTED}},
                   {navigation_ids_[0], {Metrics::Onboarding::OB_SHOWN}}})));
}

TEST_F(StarterTest, ForceOnboardingFlagForReturningUsersSucceeds) {
  SetupPlatformDelegateForReturningUser();
  base::MockCallback<base::OnceCallback<void(
      base::OnceCallback<void(bool, OnboardingResult)>)>>
      mock_onboarding_callback;
  fake_platform_delegate_.on_show_onboarding_callback_ =
      mock_onboarding_callback.Get();

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantForceOnboarding, "true");
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  EXPECT_CALL(mock_onboarding_callback, Run);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
}

TEST_F(StarterTest, ForceFirstTimeUserExperienceForReturningUser) {
  GetTriggerScriptsResponseProto get_trigger_scripts_response;
  auto* first_time_user_script =
      get_trigger_scripts_response.add_trigger_scripts();
  first_time_user_script->mutable_user_interface()->set_status_message(
      "First time user");
  first_time_user_script->mutable_trigger_condition()
      ->mutable_is_first_time_user();
  auto* returning_user_script =
      get_trigger_scripts_response.add_trigger_scripts();
  returning_user_script->mutable_user_interface()->set_status_message(
      "Returning user");
  returning_user_script->mutable_trigger_condition()
      ->mutable_none_of()
      ->add_conditions()
      ->mutable_is_first_time_user();
  std::string serialized_get_trigger_scripts_response;
  get_trigger_scripts_response.SerializeToString(
      &serialized_get_trigger_scripts_response);

  SetupPlatformDelegateForReturningUser();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantForceFirstTimeUser, "true");

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};

  SetupRpcTriggerScriptResponseOnce(kExampleDeeplink,
                                    serialized_get_trigger_scripts_response);
  EXPECT_CALL(*mock_trigger_script_ui_delegate_,
              ShowTriggerScript(first_time_user_script->user_interface()));
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
}

TEST_F(StarterTest, RegularStartupFailsIfDfmInstallationFails) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_FAILED;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_FAILED,
      1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, RegularStartupFailsIfOnboardingRejected) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::REJECTED;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink},
      {"INTENT", "SHOPPING_ASSISTED_CHECKOUT"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_THAT(fake_platform_delegate_.GetOnboardingAccepted(), Eq(false));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0], {Metrics::Onboarding::OB_CANCELLED}},
                   {navigation_ids_[0], {Metrics::Onboarding::OB_SHOWN}}})));
}

TEST_F(StarterTest, RpcTriggerScriptFailsIfMsbbIsDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = false;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));

  EXPECT_THAT(
      GetUkmTriggerScriptStarted(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptStarted::PROACTIVE_TRIGGERING_DISABLED}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, RpcTriggerScriptFailsIfProactiveHelpIsDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.proactive_help_enabled_ = false;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));

  EXPECT_THAT(
      GetUkmTriggerScriptStarted(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptStarted::PROACTIVE_TRIGGERING_DISABLED}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::FAILED_SETTING_DISABLED,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, RpcTriggerScriptSucceeds) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
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
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(
          WithArgs<1, 2>([&](const std::string& request_body,
                             ServiceRequestSender::ResponseCallback& callback) {
            // Note that trigger scripts should be fetched for the
            // ORIGINAL_DEEPLINK, not for the |initial_url|.
            GetTriggerScriptsRequestProto request;
            ASSERT_TRUE(request.ParseFromString(request_body));
            EXPECT_THAT(request.url(), Eq(GURL(kExampleDeeplink)));
            EXPECT_FALSE(request.client_context().is_in_chrome_triggered());
            std::move(callback).Run(
                net::HTTP_OK,
                CreateTriggerScriptResponseForTest(
                    TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER),
                ServiceRequestSender::ResponseInfo{});
          }));
  GetTriggerScriptsResponseProto get_trigger_scripts_response;
  get_trigger_scripts_response.ParseFromString(
      CreateTriggerScriptResponseForTest(
          TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER));
  EXPECT_CALL(
      mock_start_regular_script_callback_,
      Run(GURL(kExampleDeeplink),
          Pointee(AllOf(
              Property(&TriggerContext::GetOnboardingShown, true),
              Property(&TriggerContext::GetInChromeTriggered, false),
              Property(&TriggerContext::GetTriggerUIType,
                       TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER))),
          Optional(get_trigger_scripts_response.trigger_scripts(0))));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptStarted::FIRST_TIME_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                     TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER}}})));
  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_ACCEPTED,
             TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER}}})));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::OK_DELAYED_START,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, CancelPendingTriggerScriptWhenTransitioningFromCctToTab) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_custom_tab_ = true;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};

  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  SetupRpcTriggerScriptResponseOnce(kExampleDeeplink);

  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));
  starter_->Init();
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, HideTriggerScript);
  fake_platform_delegate_.is_custom_tab_ = false;
  starter_->Init();
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptStarted::RETURNING_USER}}})));
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::CCT_TO_TAB_NOT_SUPPORTED,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, CancelPendingTriggerScriptWhenHandlingNewStartupRequest) {
  SetupPlatformDelegateForReturningUser();

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};

  SetupRpcTriggerScriptResponseOnce(kExampleDeeplink);

  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  EXPECT_CALL(*mock_trigger_script_ui_delegate_, HideTriggerScript);
  PrepareTriggerScriptUiDelegate();
  PrepareTriggerScriptRequestSender();
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::CANCELED,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(StarterTest, RegularStartupFailsIfNavigationDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();

  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  SimulateNavigateToUrl(GURL("https://www.different.com"));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0], {Metrics::Onboarding::OB_NO_ANSWER}},
                   {navigation_ids_[0], {Metrics::Onboarding::OB_SHOWN}}})));
}

TEST_F(StarterTest, TriggerScriptStartupFailsIfNavigationDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};

  SetupRpcTriggerScriptResponseOnce(
      kExampleDeeplink, CreateTriggerScriptResponseForTest(
                            TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  SimulateNavigateToUrl(GURL("https://www.different.com"));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptStarted::FIRST_TIME_USER}}})));
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE,
             TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptOnboarding::
                         ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION,
                     TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER}}})));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, RegularStartupAllowsCertainNavigationsDuringOnboarding) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  // Expect that the onboarding is not interrupted by a navigation to a
  // subdomain of the ORIGINAL_DEEPLINK, nor by redirects along the way.
  SimulateRedirectToUrl(
      {GURL("http://redirect.com/example"), GURL("https://login.example.com")});
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());

  // Navigating to a different domain will cancel the onboarding.
  SimulateNavigateToUrl(GURL("https://www.different.com"));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[1], {Metrics::Onboarding::OB_NO_ANSWER}},
                   {navigation_ids_[1], {Metrics::Onboarding::OB_SHOWN}}})));
}

TEST_F(StarterTest, TriggerScriptAllowsHttpToHttpsRedirect) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "http://www.example.com"}};

  SetupRpcTriggerScriptResponseOnce("http://www.example.com");
  SimulateNavigateToUrl(GURL("http://www.example.com"));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript).Times(1);
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, HideTriggerScript).Times(0);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  // Redirects from http to https are allowed.
  SimulateRedirectToUrl({GURL("https://www.example.com")});
}

TEST_F(StarterTest, RegularStartupIgnoresLastCommittedUrl) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();

  // Note: the starter does not actually care about the last committed URL at
  // the time of startup. All that matters is that it has received the startup
  // intent, and that there is a valid ORIGINAL_DEEPLINK to expect.
  SimulateNavigateToUrl(GURL("https://www.ignored.com"));
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, ImplicitStartupOnSupportedDomainWithoutLogin) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = true;
  fake_platform_delegate_.proactive_help_enabled_ = true;
  fake_platform_delegate_.is_logged_in_ = false;
  fake_platform_delegate_.is_web_layer_ = false;
  starter_->Init();

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(
          WithArgs<1, 2>([&](const std::string& request_body,
                             ServiceRequestSender::ResponseCallback& callback) {
            GetTriggerScriptsRequestProto request;
            ASSERT_TRUE(request.ParseFromString(request_body));
            EXPECT_THAT(request.url(),
                        Eq(GURL("https://www.some-website.com/cart")));
            EXPECT_TRUE(request.client_context().is_in_chrome_triggered());
            std::move(callback).Run(
                net::HTTP_OK,
                CreateTriggerScriptResponseForTest(
                    TriggerScriptProto::SHOPPING_CART_RETURNING_USER),
                ServiceRequestSender::ResponseInfo{});
          }));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  EXPECT_CALL(
      mock_start_regular_script_callback_,
      Run(GURL("https://www.some-website.com/cart"),
          Pointee(AllOf(
              Property(&TriggerContext::GetOnboardingShown, false),
              Property(&TriggerContext::GetInChromeTriggered, true),
              Property(&TriggerContext::GetTriggerUIType,
                       TriggerScriptProto::SHOPPING_CART_RETURNING_USER))),
          testing::Ne(absl::nullopt)));

  // Implicit startup by navigating to an autofill-assistant-enabled site.
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[1],
                    {Metrics::TriggerScriptStarted::RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[1],
                    {Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[1],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_ALREADY_ACCEPTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH}},
           {navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}}})));

  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, DoNotStartImplicitlyIfSettingDisabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.proactive_help_enabled_ = false;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(GetUkmInChromeTriggering(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, DoNotStartImplicitlyForNonAgaCct) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_tab_created_by_gsa_ = false;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(GetUkmInChromeTriggering(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, DoNotStartImplicitlyForWebLayer) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_custom_tab_ = false;
  fake_platform_delegate_.is_web_layer_ = true;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(GetUkmInChromeTriggering(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, ImplicitStartupOnCurrentUrlAfterSettingEnabled) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.proactive_help_enabled_ = false;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript).Times(1);

  // Implicit startup by enabling proactive help while already on an
  // autofill-assistant-enabled site.
  fake_platform_delegate_.proactive_help_enabled_ = true;
  starter_->Init();
  task_environment()->RunUntilIdle();

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[1],
                    {Metrics::TriggerScriptStarted::RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}}})));
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, StartTriggerScriptBeforeRedirectRecordsUkmForTargetUrl) {
  SetupPlatformDelegateForReturningUser();

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";

  SetupRpcTriggerScriptResponseOnce(kExampleDeeplink);

  // Simulate a real flow that starts on some trigger site, which then redirects
  // to the deeplink.
  SimulateNavigateToUrl(GURL("https://some-trigger-site.com"));
  // Start the flow before the trigger site has had a chance to navigate to the
  // target domain. This commonly happens due to android intent handling
  // happening before navigations are started.
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript);

  SimulateRedirectToUrl({GURL("https://redirect.com/to/www/example/com"),
                         GURL(kExampleDeeplink),
                         GURL("https://signin.example.com")});
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[2],
                    {Metrics::TriggerScriptStarted::RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, RedirectFailsDuringPendingTriggerScriptStart) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.feature_module_installed_ = true;
  fake_platform_delegate_.trigger_script_request_sender_for_test_ = nullptr;
  mock_trigger_script_service_request_sender_ = nullptr;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";

  // Simulate a real flow that starts on some trigger site, which then redirects
  // to the deeplink.
  SimulateNavigateToUrl(GURL("https://some-trigger-site.com"));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript).Times(0);

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          web_contents()->GetLastCommittedURL(),
          web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  simulator->Redirect(GURL("https://redirect.com/to/www/example/com"));
  simulator->Fail(net::ERR_BLOCKED_BY_CLIENT);
  simulator->CommitErrorPage();
  navigation_ids_.emplace_back(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  // Note that this impression is recorded for the last URL that a navigation-
  // start event occurred for. We never reached the target domain, so this is
  // unfortunately the best we can do.
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[2],
                    {Metrics::TriggerScriptStarted::NAVIGATION_ERROR}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, StartTriggerScriptDuringRedirectRecordsUkmForTargetUrl) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.feature_module_installed_ = true;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";

  // Simulate a real flow that starts on some trigger site, which then redirects
  // to the deeplink.
  SimulateNavigateToUrl(GURL("https://some-trigger-site.com"));

  SetupRpcTriggerScriptResponseOnce(kExampleDeeplink);
  // Begin a navigation, then start the flow before the navigation is committed.
  // UKM should still be recorded for the final URL, not the redirect URL.
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript);
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          web_contents()->GetLastCommittedURL(),
          web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  simulator->Redirect(GURL("https://redirect.com/to/www/example/com"));
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));
  simulator->Redirect(GURL(kExampleDeeplink));
  simulator->Commit();
  navigation_ids_.emplace_back(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[2],
                    {Metrics::TriggerScriptStarted::RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, RegularStartupDoesNotWaitForNavigationToFinish) {
  SetupPlatformDelegateForReturningUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;

  SimulateNavigateToUrl(GURL("https://some-trigger-site.com"));
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          web_contents()->GetLastCommittedURL(),
          web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  simulator->Redirect(GURL("https://redirect.com/to/www/example/com"));

  {
    EXPECT_CALL(mock_start_regular_script_callback_,
                Run(GURL(kExampleDeeplink), _, _));
    starter_->Start(std::make_unique<TriggerContext>(
        std::make_unique<ScriptParameters>(script_parameters), options));
  }

  simulator->Redirect(GURL(kExampleDeeplink));
  simulator->Commit();

  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(
      GetUkmRegularScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[1], {Metrics::Onboarding::OB_NOT_SHOWN}}})));
}

TEST_F(StarterTest, DoNotStartImplicitlyIfAlreadyRunning) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_regular_script_running_ = true;
  starter_->Init();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;

  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));

  task_environment()->RunUntilIdle();
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmInChromeTriggering(ukm_recorder_), IsEmpty());

  histogram_tester_.ExpectTotalCount(
      "Android.AutofillAssistant.FeatureModuleInstallation", 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, FailedTriggerScriptFetchesForImplicitStartupAreCached) {
  SetupPlatformDelegateForReturningUser();
  starter_->Init();

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript).Times(0);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  // Implicit startup by navigating to an autofill-assistant-enabled site.
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(*GetFailedTriggerFetchesCacheForTest(),
              UnorderedElementsAre(
                  Pair("some-website.com",
                       task_environment()->GetMockTickClock()->NowTicks())));

  // Since we failed to communicate with the backend, we won't try again for the
  // same domain or sub-domain.
  PrepareTriggerScriptRequestSender();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/checkout"));
  SimulateNavigateToUrl(GURL("https://some-website.com/signin"));
  SimulateNavigateToUrl(GURL("https://signin.some-website.com"));
  task_environment()->RunUntilIdle();

  // Navigations to different autofill-assistant-enabled URLs will still trigger
  // implicit startup.
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(1);
  SimulateNavigateToUrl(GURL("https://www.different-website.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH}},
           {navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}},
           {navigation_ids_[2],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}},
           {navigation_ids_[3],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}},
           {navigation_ids_[4],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}},
           {navigation_ids_[5],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}}})));
}

TEST_F(StarterTest,
       CancelingTriggerScriptsDenylistsTheDomainForImplicitStartup) {
  SetupPlatformDelegateForReturningUser();
  starter_->Init();

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::CANCEL_SESSION);
      });

  // Implicit startup by navigating to an autofill-assistant-enabled site.
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(*GetUserDenylistedCacheForTest(),
              UnorderedElementsAre(
                  Pair("some-website.com",
                       task_environment()->GetMockTickClock()->NowTicks())));

  // Since the user chose CANCEL_SESSION, subsequent navigations to the same
  // domain or sub-domains should not trigger implicit startup.
  PrepareTriggerScriptRequestSender();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/checkout"));
  SimulateNavigateToUrl(GURL("https://some-website.com/signin"));
  SimulateNavigateToUrl(GURL("https://signin.some-website.com"));
  task_environment()->RunUntilIdle();

  // Navigations to different autofill-assistant-enabled URLs will still trigger
  // implicit startup.
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(1);
  SimulateNavigateToUrl(GURL("https://www.different-website.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH}},
           {navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}},
           {navigation_ids_[2],
            {Metrics::InChromeTriggerAction::USER_DENYLISTED_DOMAIN}},
           {navigation_ids_[3],
            {Metrics::InChromeTriggerAction::USER_DENYLISTED_DOMAIN}},
           {navigation_ids_[4],
            {Metrics::InChromeTriggerAction::USER_DENYLISTED_DOMAIN}},
           {navigation_ids_[5],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}}})));
}

TEST_F(StarterTest, EmptyTriggerScriptFetchesForImplicitStartupAreCached) {
  SetupPlatformDelegateForReturningUser();
  starter_->Init();

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(
          WithArg<2>([&](ServiceRequestSender::ResponseCallback& callback) {
            // Empty response == no trigger scripts available.
            std::move(callback).Run(net::HTTP_OK, std::string(),
                                    ServiceRequestSender::ResponseInfo{});
          }));

  // Implicit startup by navigating to an autofill-assistant-enabled site.
  SimulateNavigateToUrl(GURL("https://www.some-website.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(*GetFailedTriggerFetchesCacheForTest(),
              UnorderedElementsAre(
                  Pair("some-website.com",
                       task_environment()->GetMockTickClock()->NowTicks())));

  // Subsequent navigations to the same domain should not talk to the backend
  // again. This includes navigations to subdomains etc.
  PrepareTriggerScriptRequestSender();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.some-website.com/checkout"));
  SimulateNavigateToUrl(GURL("https://some-website.com/signin"));
  SimulateNavigateToUrl(GURL("https://signin.some-website.com"));
  task_environment()->RunUntilIdle();

  // However, explicit requests still communicate with the backend.
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(1);
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.some-website.com/cart"}};
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));

  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH}},
           {navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}},
           {navigation_ids_[2],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}},
           {navigation_ids_[3],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}},
           {navigation_ids_[4],
            {Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN}}})));
}

TEST_F(StarterTest, FailedExplicitTriggerFetchesAreCached) {
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"}};
  std::vector<std::string> unsupported_sites = {
      "https://www.example.com", "https://signing.example.com",
      "https://different.com", "https://different.com/test?q=12345"};
  for (const auto& url : unsupported_sites) {
    SimulateNavigateToUrl(GURL(url));
    PrepareTriggerScriptRequestSender();
    PrepareTriggerScriptUiDelegate();
    // Send empty response == no trigger script available. Note that explicit
    // start requests like these will always attempt to talk to the backend, no
    // matter the cache contents.
    EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                     ServiceRequestSender::ResponseInfo{}));
    script_parameters["ORIGINAL_DEEPLINK"] = url;
    starter_->Start(std::make_unique<TriggerContext>(
        std::make_unique<ScriptParameters>(script_parameters),
        TriggerContext::Options{}));
  }
  // Cache should contain one entry per organization-identifying domain.
  base::TimeTicks now_ticks =
      task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_THAT(*GetFailedTriggerFetchesCacheForTest(),
              UnorderedElementsAre(Pair("example.com", now_ticks),
                                   Pair("different.com", now_ticks)));
  EXPECT_THAT(GetUkmInChromeTriggering(ukm_recorder_), IsEmpty());
}

TEST_F(StarterTest, FailedImplicitTriggerFetchesAreCached) {
  starter_->Init();

  std::vector<std::string> implicit_unsupported_sites = {
      "https://www.example-shopping-site.com/cart",
      "https://different-shopping-site.com/cart"};
  for (const auto& url : implicit_unsupported_sites) {
    PrepareTriggerScriptRequestSender();
    PrepareTriggerScriptUiDelegate();
    // Send empty response == no trigger script available.
    EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                     ServiceRequestSender::ResponseInfo{}));
    SimulateNavigateToUrl(GURL(url));
    task_environment()->RunUntilIdle();
  }
  // Failed attempts to start implicitly are added to the cache.
  base::TimeTicks now_ticks =
      task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_THAT(
      *GetFailedTriggerFetchesCacheForTest(),
      UnorderedElementsAre(Pair("example-shopping-site.com", now_ticks),
                           Pair("different-shopping-site.com", now_ticks)));
  EXPECT_THAT(
      GetUkmInChromeTriggering(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH}},
           {navigation_ids_[1],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}},
           {navigation_ids_[2],
            {Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED}}})));
}

TEST_F(StarterTest, FailedTriggerFetchesCacheEntriesExpire) {
  starter_->Init();

  GetFailedTriggerFetchesCacheForTest()->Put(
      "example.com", task_environment()->GetMockTickClock()->NowTicks());
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));
  task_environment()->FastForwardBy(base::Hours(1));
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  // Since the request failed again, the cache entry should be updated with the
  // new time.
  EXPECT_THAT(
      *GetFailedTriggerFetchesCacheForTest(),
      UnorderedElementsAre(Pair(
          "example.com", task_environment()->GetMockTickClock()->NowTicks())));
}

TEST_F(StarterTest, UserDenylistedCacheUpdateAndExpire) {
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::CANCEL_SESSION);
      });
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();
  EXPECT_THAT(
      *GetUserDenylistedCacheForTest(),
      UnorderedElementsAre(Pair(
          "example.com", task_environment()->GetMockTickClock()->NowTicks())));

  PrepareTriggerScriptRequestSender();
  PrepareTriggerScriptUiDelegate();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::CANCEL_SESSION);
      });
  task_environment()->FastForwardBy(base::Hours(1));
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  // Since the request was cancelled again, the cache entry should have been
  // updated with the new time.
  EXPECT_THAT(
      *GetUserDenylistedCacheForTest(),
      UnorderedElementsAre(Pair(
          "example.com", task_environment()->GetMockTickClock()->NowTicks())));
}

TEST_F(StarterTest, RemoveEntryFromCacheOnSuccessForExplicitRequest) {
  starter_->Init();
  GetFailedTriggerFetchesCacheForTest()->Put(
      "example.com", task_environment()->GetMockTickClock()->NowTicks());

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::ACCEPT);
      });
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", "https://www.example.com"}};
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{}));
  EXPECT_THAT(*GetFailedTriggerFetchesCacheForTest(), IsEmpty());
}

TEST_F(StarterTest, ImplicitInCctTriggeringSmokeTest) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_custom_tab_ = true;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();
}

TEST_F(StarterTest, ImplicitInCctTriggeringDoesNotTriggerInTab) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_custom_tab_ = false;
  starter_->Init();

  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();
}

TEST_F(StarterTest, RecordsDependenciesInvalidatedOutsideFlow) {
  starter_->OnDependenciesInvalidated();

  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.DependenciesInvalidated",
      Metrics::DependenciesInvalidated::OUTSIDE_FLOW, 1);
}

TEST_F(StarterTest, RecordsDependenciesInvalidatedDuringStartup) {
  SetupPlatformDelegateForFirstTimeUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"}, {"START_IMMEDIATELY", "true"}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  starter_->OnDependenciesInvalidated();

  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.DependenciesInvalidated",
      Metrics::DependenciesInvalidated::DURING_STARTUP, 1);
}

TEST_F(StarterTest, RecordsDependenciesInvalidatedDuringFlow) {
  fake_platform_delegate_.is_regular_script_running_ = true;

  starter_->OnDependenciesInvalidated();

  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.DependenciesInvalidated",
      Metrics::DependenciesInvalidated::DURING_FLOW, 1);
}

TEST(MultipleStarterTest, HeuristicUsedByMultipleInstances) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  content::RenderViewHostTestEnabler rvh_test_enabler;
  content::TestBrowserContext browser_context;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  FakeStarterPlatformDelegate fake_platform_delegate_01 =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));
  FakeStarterPlatformDelegate fake_platform_delegate_02 =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));
  MockRuntimeManager mock_runtime_manager;

  auto web_contents_01 = content::WebContentsTester::CreateTestWebContents(
      &browser_context, nullptr);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents_01.get());
  auto web_contents_02 = content::WebContentsTester::CreateTestWebContents(
      &browser_context, nullptr);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents_02.get());

  fake_platform_delegate_01.fake_common_dependencies_->permanent_country_code_ =
      "us";
  fake_platform_delegate_02.fake_common_dependencies_->permanent_country_code_ =
      "us";

  Starter starter_01(web_contents_01.get(),
                     fake_platform_delegate_01.GetWeakPtr(), &ukm_recorder,
                     mock_runtime_manager.GetWeakPtr(),
                     task_environment.GetMockTickClock());
  starter_01.Init();
  Starter starter_02(web_contents_02.get(),
                     fake_platform_delegate_02.GetWeakPtr(), &ukm_recorder,
                     mock_runtime_manager.GetWeakPtr(),
                     task_environment.GetMockTickClock());
  starter_02.Init();

  auto service_request_sender_01 =
      std::make_unique<NiceMock<MockServiceRequestSender>>();
  auto* service_request_sender_01_ptr = service_request_sender_01.get();
  fake_platform_delegate_01.trigger_script_request_sender_for_test_ =
      std::move(service_request_sender_01);
  auto service_request_sender_02 =
      std::make_unique<NiceMock<MockServiceRequestSender>>();
  auto* service_request_sender_02_ptr = service_request_sender_02.get();
  fake_platform_delegate_02.trigger_script_request_sender_for_test_ =
      std::move(service_request_sender_02);

  EXPECT_CALL(*service_request_sender_01_ptr, OnSendRequest).Times(1);
  EXPECT_CALL(*service_request_sender_02_ptr, OnSendRequest).Times(1);
  content::WebContentsTester::For(web_contents_01.get())
      ->NavigateAndCommit(GURL("https://www.some-website.com/cart"));
  content::WebContentsTester::For(web_contents_02.get())
      ->NavigateAndCommit(GURL("https://www.some-other-website.com/cart"));
  task_environment.RunUntilIdle();
}

TEST_F(StarterTest, StaleCacheEntriesAreRemovedOnInsertingNewEntries) {
  starter_->Init();
  base::TimeTicks t0 = task_environment()->GetMockTickClock()->NowTicks();
  GetFailedTriggerFetchesCacheForTest()->Put("failed-t0.com", t0);
  GetUserDenylistedCacheForTest()->Put("denylisted-t0.com", t0);

  task_environment()->FastForwardBy(base::Minutes(30));
  base::TimeTicks t1 = task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_THAT(*GetFailedTriggerFetchesCacheForTest(),
              UnorderedElementsAre(Pair("failed-t0.com", t0)));
  EXPECT_THAT(*GetUserDenylistedCacheForTest(),
              UnorderedElementsAre(Pair("denylisted-t0.com", t0)));
  GetFailedTriggerFetchesCacheForTest()->Put("failed-t1.com", t1);
  GetUserDenylistedCacheForTest()->Put("denylisted-t1.com", t1);

  task_environment()->FastForwardBy(base::Minutes(30));
  base::TimeTicks t2 = task_environment()->GetMockTickClock()->NowTicks();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));
  SimulateNavigateToUrl(GURL("https://www.example.com/cart"));
  task_environment()->RunUntilIdle();

  // failed-t0.com should have been removed from the cache due to going stale.
  EXPECT_THAT(
      *GetFailedTriggerFetchesCacheForTest(),
      UnorderedElementsAre(Pair("failed-t1.com", t1), Pair("example.com", t2)));
  // denylisted-t0.com is stale and will be removed the next time a domain is
  // denylisted.
  EXPECT_THAT(*GetUserDenylistedCacheForTest(),
              UnorderedElementsAre(Pair("denylisted-t0.com", t0),
                                   Pair("denylisted-t1.com", t1)));

  PrepareTriggerScriptRequestSender();
  PrepareTriggerScriptUiDelegate();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK,
                                   CreateTriggerScriptResponseForTest(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, ShowTriggerScript)
      .WillOnce([&]() {
        ASSERT_TRUE(trigger_script_coordinator_ != nullptr);
        trigger_script_coordinator_->PerformTriggerScriptAction(
            TriggerScriptProto::CANCEL_SESSION);
      });
  SimulateNavigateToUrl(GURL("https://supported.com/cart"));
  task_environment()->RunUntilIdle();

  // No change to the failed fetches cache.
  EXPECT_THAT(
      *GetFailedTriggerFetchesCacheForTest(),
      UnorderedElementsAre(Pair("failed-t1.com", t1), Pair("example.com", t2)));
  // denylisted-t0.com should have been removed due to going stale.
  EXPECT_THAT(*GetUserDenylistedCacheForTest(),
              UnorderedElementsAre(Pair("denylisted-t1.com", t1),
                                   Pair("supported.com", t2)));
}

TEST_F(StarterTest, CommandLineScriptParametersAreAddedToImplicitTriggers) {
  ImplicitTriggeringDebugParametersProto proto;
  auto* param = proto.add_additional_script_parameters();
  param->set_name("DEBUG_SOCKET_ID");
  param->set_value("FAKE_SOCKET_ID");

  param = proto.add_additional_script_parameters();
  param->set_name("DEBUG_BUNDLE_ID");
  param->set_value("FAKE_BUNDLE_ID");

  param = proto.add_additional_script_parameters();
  param->set_name("INTENT");
  param->set_value("NEW_INTENT");

  param = proto.add_additional_script_parameters();
  param->set_name("NOT_ALLOWLISTED");
  param->set_value("SHOULD_NOT_BE_SENT_TO_BACKEND");

  std::string implicit_triggering_debug_parameters;
  proto.SerializeToString(&implicit_triggering_debug_parameters);
  base::Base64UrlEncode(implicit_triggering_debug_parameters,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &implicit_triggering_debug_parameters);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantImplicitTriggeringDebugParameters,
      implicit_triggering_debug_parameters);

  // Create new instance of the starter to force the changed command line to
  // take effect.
  starter_ = std::make_unique<Starter>(
      web_contents(), fake_platform_delegate_.GetWeakPtr(), &ukm_recorder_,
      mock_runtime_manager_.GetWeakPtr(),
      task_environment()->GetMockTickClock());
  starter_->Init();

  EXPECT_CALL(
      *mock_trigger_script_service_request_sender_,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(WithArg<1>([&](const std::string& request_body) {
        GetTriggerScriptsRequestProto request;
        ASSERT_TRUE(request.ParseFromString(request_body));
        EXPECT_THAT(request.url(), Eq(GURL("https://example.com/cart")));
        EXPECT_THAT(
            request.script_parameters(),
            UnorderedElementsAre(
                AllOf(Property(&ScriptParameterProto::name, "DEBUG_SOCKET_ID"),
                      Property(&ScriptParameterProto::value, "FAKE_SOCKET_ID")),
                AllOf(Property(&ScriptParameterProto::name, "DEBUG_BUNDLE_ID"),
                      Property(&ScriptParameterProto::value, "FAKE_BUNDLE_ID")),
                AllOf(Property(&ScriptParameterProto::name, "INTENT"),
                      Property(&ScriptParameterProto::value, "NEW_INTENT"))));
      }));

  SimulateNavigateToUrl(GURL("https://example.com/cart"));
  task_environment()->RunUntilIdle();
}

TEST(MultipleIntentStarterTest, ImplicitTriggeringForShoppingAndCoupons) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  content::RenderViewHostTestEnabler rvh_test_enabler;
  content::TestBrowserContext browser_context;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  FakeStarterPlatformDelegate fake_platform_delegate =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));
  fake_platform_delegate.fake_common_dependencies_->permanent_country_code_ =
      "us";
  MockRuntimeManager mock_runtime_manager;

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      &browser_context, nullptr);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents.get());

  Starter starter(web_contents.get(), fake_platform_delegate.GetWeakPtr(),
                  &ukm_recorder, mock_runtime_manager.GetWeakPtr(),
                  task_environment.GetMockTickClock());
  starter.Init();
  auto service_request_sender =
      std::make_unique<NiceMock<MockServiceRequestSender>>();
  auto* service_request_sender_ptr = service_request_sender.get();
  fake_platform_delegate.trigger_script_request_sender_for_test_ =
      std::move(service_request_sender);

  EXPECT_CALL(
      *service_request_sender_ptr,
      OnSendRequest(GURL("https://automate-pa.googleapis.com/v1/triggers"), _,
                    _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(WithArg<1>([&](const std::string& request_body) {
        GetTriggerScriptsRequestProto request;
        ASSERT_TRUE(request.ParseFromString(request_body));
        EXPECT_THAT(request.url(), Eq(GURL("https://example.com/cart")));
        EXPECT_TRUE(request.client_context().is_in_chrome_triggered());
        const auto actual_intent_param = base::ranges::find(
            request.script_parameters(), "INTENT", &ScriptParameterProto::name);
        ASSERT_THAT(actual_intent_param, Ne(request.script_parameters().end()));
        auto actual_intents =
            base::SplitString(actual_intent_param->value(), ",",
                              base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        EXPECT_THAT(
            actual_intents,
            UnorderedElementsAre("SHOPPING_ASSISTED_CHECKOUT", "FIND_COUPONS"));
      }));

  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com/cart"));
  task_environment.RunUntilIdle();
}

class StarterPrerenderTest : public StarterTest {
 public:
  StarterPrerenderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~StarterPrerenderTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(StarterPrerenderTest, DoNotAffectRecordUkmDuringPrendering) {
  SetupPlatformDelegateForFirstTimeUser();
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  fake_platform_delegate_.feature_module_installed_ = true;
  // Empty callback to keep the onboarding open indefinitely.
  fake_platform_delegate_.on_show_onboarding_callback_ = base::DoNothing();

  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  TriggerContext::Options options;
  options.initial_url = kExampleDeeplink;
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};

  SimulateNavigateToUrl(GURL("https://www.different.com"));

  // Trigger scripts wait for navigation to the deeplink domain.
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));

  // Start prerendering a page.
  const GURL prerendering_url("https://www.different.com/prerendering");
  auto* prerender_rfh = content::WebContentsTester::For(web_contents())
                            ->AddPrerenderAndCommitNavigation(prerendering_url);
  ASSERT_TRUE(prerender_rfh);

  // Prerendering should not affect any trigger script's behaviour and not
  // record anything.
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 0u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_), IsEmpty());
}

class StarterFencedFrameTest : public StarterTest {
 public:
  StarterFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~StarterFencedFrameTest() override = default;

  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* fenced_frame =
        content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(StarterFencedFrameTest, NoImplicitTriggeringForFencedFrames) {
  // This test should not fetch trigger scripts.
  SetupPlatformDelegateForReturningUser();
  PrepareTriggerScriptRequestSender();
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};

  // Start on "different.com" and it shouldn't start immediately.
  SimulateNavigateToUrl(GURL("https://www.different.com"));
  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
  // Create a fenced frame and navigate to the example deeplink. The navigation
  // should not start because it is not in the primary main frame.
  content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(web_contents()->GetPrimaryMainFrame());
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kExampleDeeplink), fenced_frame_rfh);
  navigation_simulator->Start();
}

TEST_F(StarterTest, StartupRegistersTriggerFieldTrial) {
  auto mock_field_trial_util =
      std::make_unique<NiceMock<MockAssistantFieldTrialUtil>>();
  const auto* mock_field_trial_util_ptr = mock_field_trial_util.get();
  fake_platform_delegate_.field_trial_util_ = std::move(mock_field_trial_util);

  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantTriggered"),
                  base::StringPiece("Enabled")));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(), TriggerContext::Options()));
}

TEST_F(StarterTest, StartupRegistersExperimentFieldTrials) {
  base::flat_map<std::string, std::string> script_parameters = {
      {"EXPERIMENT_IDS", "1002,1001"},
      {"FIELD_TRIAL_2", "1002"},
      {"FIELD_TRIAL_1", "1001"}};
  auto mock_field_trial_util =
      std::make_unique<NiceMock<MockAssistantFieldTrialUtil>>();
  const auto* mock_field_trial_util_ptr = mock_field_trial_util.get();
  fake_platform_delegate_.field_trial_util_ = std::move(mock_field_trial_util);

  EXPECT_CALL(*mock_field_trial_util_ptr, RegisterSyntheticFieldTrial);

  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial-1"),
                  base::StringPiece("1001")));

  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial-2"),
                  base::StringPiece("1002")));

  // Backwards compatibility.
  // TODO(b/242171397): Remove
  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial"),
                  base::StringPiece("1001")));

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
}

TEST_F(StarterTest, CanStartSucceeds) {
  SetupPlatformDelegateForReturningUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  base::MockCallback<base::OnceCallback<void(
      bool success, const OnboardingState& onboarding_state,
      absl::optional<GURL> url,
      std::unique_ptr<TriggerContext> trigger_context)>>
      mock_preconditions_checked_callback;
  EXPECT_CALL(mock_preconditions_checked_callback,
              Run(/*success=*/true,
                  OnboardingState{
                      /*onboarding_shown=*/false, /*onboarding_skipped=*/false,
                      /*onboarding_result=*/OnboardingResult::ACCEPTED},
                  Optional(GURL(kExampleDeeplink)), _));

  starter_->CanStart(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_preconditions_checked_callback.Get());

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(0));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED, 1u);
  EXPECT_THAT(
      GetUkmRegularScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0], {Metrics::Onboarding::OB_NOT_SHOWN}}})));
  EXPECT_THAT(GetUkmStartRequest(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::AutofillAssistantStarted::OK_IMMEDIATE_START,
                     AutofillAssistantIntent::UNDEFINED_INTENT,
                     Metrics::AutofillAssistantCaller::UNKNOWN_CALLER,
                     Metrics::AutofillAssistantSource::UNKNOWN_SOURCE,
                     Metrics::AutofillAssistantExperiment::NO_EXPERIMENT}}})));
}

TEST_F(StarterTest, FirstTimeUserNotShowOnboardingIfHandledExternally) {
  SetupPlatformDelegateForFirstTimeUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  options.is_externally_triggered = true;
  options.skip_autofill_assistant_onboarding = true;
  base::MockCallback<base::OnceCallback<void(
      bool success, const OnboardingState& onboarding_state,
      absl::optional<GURL> url,
      std::unique_ptr<TriggerContext> trigger_context)>>
      mock_preconditions_checked_callback;
  EXPECT_CALL(mock_preconditions_checked_callback,
              Run(/*success=*/true,
                  OnboardingState{/*onboarding_shown=*/false,
                                  /*onboarding_skipped=*/true,
                                  /*onboarding_result=*/absl::nullopt},
                  Optional(GURL(kExampleDeeplink)), _));

  starter_->CanStart(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_preconditions_checked_callback.Get());

  EXPECT_THAT(fake_platform_delegate_.num_install_feature_module_called_,
              Eq(1));
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(0));
  // Prerendering should not affect any trigger script's behaviour and not
  // record anything.
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED,
      1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0], {Metrics::Onboarding::OB_EXTERNAL}}})));
}

TEST_F(StarterTest,
       FirstTimeUserShowOnboardingIfHandledExternallyWithUseOnboardingFlag) {
  SetupPlatformDelegateForFirstTimeUser();
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  options.is_externally_triggered = true;
  options.skip_autofill_assistant_onboarding = false;
  base::MockCallback<base::OnceCallback<void(
      bool success, const OnboardingState& onboarding_state,
      absl::optional<GURL> url,
      std::unique_ptr<TriggerContext> trigger_context)>>
      mock_preconditions_checked_callback;
  EXPECT_CALL(mock_preconditions_checked_callback,
              Run(/*success=*/true,
                  OnboardingState{
                      /*onboarding_shown=*/true, /*onboarding_skipped=*/false,
                      /*onboarding_result=*/OnboardingResult::ACCEPTED},
                  Optional(GURL(kExampleDeeplink)), _));

  starter_->CanStart(
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(script_parameters), options),
      mock_preconditions_checked_callback.Get());

  EXPECT_EQ(fake_platform_delegate_.num_install_feature_module_called_, 1);
  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_TRUE(fake_platform_delegate_.GetOnboardingAccepted());
  EXPECT_THAT(GetUkmTriggerScriptStarted(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
  EXPECT_THAT(GetUkmTriggerScriptOnboarding(ukm_recorder_), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Android.AutofillAssistant.FeatureModuleInstallation",
      Metrics::FeatureModuleInstallation::DFM_FOREGROUND_INSTALLATION_SUCCEEDED,
      1u);
  EXPECT_THAT(GetUkmRegularScriptOnboarding(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0], {Metrics::Onboarding::OB_ACCEPTED}},
                   {navigation_ids_[0], {Metrics::Onboarding::OB_SHOWN}}})));
}

TEST_F(StarterTest, RegularStartupFailsForSupervisedUser) {
  SetupPlatformDelegateForFirstTimeUser();
  fake_platform_delegate_.is_supervised_user_ = true;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));
}

TEST_F(StarterTest, RpcTriggerScriptStartupFailsForSupervisedUser) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_supervised_user_ = true;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
}

TEST_F(StarterTest, RegularStartupFailsForNotAllowedForMachineLearningUsers) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_allowed_for_machine_learning_ = false;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  TriggerContext::Options options;
  options.initial_url = "https://redirect.com/to/www/example/com";
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters), options));
}

TEST_F(StarterTest,
       RpcTriggerScriptStartupFailsForAllowedForMachineLearningUsers) {
  SetupPlatformDelegateForReturningUser();
  fake_platform_delegate_.is_allowed_for_machine_learning_ = false;

  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  EXPECT_CALL(*mock_trigger_script_ui_delegate_, Attach).Times(0);
  EXPECT_CALL(*mock_trigger_script_service_request_sender_, OnSendRequest)
      .Times(0);
  EXPECT_CALL(mock_start_regular_script_callback_, Run).Times(0);

  starter_->Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options()));
}

}  // namespace autofill_assistant
