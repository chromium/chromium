// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_controller_observer.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

class MockAnnotateDomModelService : public AnnotateDomModelService {
 public:
  MockAnnotateDomModelService() : AnnotateDomModelService(nullptr, nullptr) {}
  ~MockAnnotateDomModelService() override = default;

  MOCK_METHOD1(SetOverridesPolicy, bool(SemanticSelectorPolicy));
};

class ControllerTest : public testing::Test {
 public:
  ControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillAssistantChromeEntry);
  }

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    auto web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = web_controller.get();
    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents_.get());
    navigation_ids_.emplace_back(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

    ON_CALL(mock_client_, GetWebContents).WillByDefault(Return(web_contents()));
    ON_CALL(mock_client_, HasHadUI()).WillByDefault(Return(true));
    ON_CALL(mock_client_, GetPasswordChangeSuccessTracker())
        .WillByDefault(Return(&mock_password_change_success_tracker_));

    mock_runtime_manager_ = std::make_unique<MockRuntimeManager>();
    controller_ = std::make_unique<Controller>(
        web_contents(), &mock_client_, task_environment()->GetMockTickClock(),
        mock_runtime_manager_->GetWeakPtr(), std::move(service), &ukm_recorder_,
        &mock_annotate_dom_model_service_);

    SetWebControllerForTest(controller_.get(), std::move(web_controller));

    ON_CALL(mock_client_, AttachUI()).WillByDefault(Invoke([this]() {
      controller_->SetUiShown(true);
    }));

    ON_CALL(mock_client_, DestroyUI()).WillByDefault(Invoke([this]() {
      controller_->SetUiShown(false);
    }));
    ON_CALL(mock_client_, GetScriptExecutorUiDelegate())
        .WillByDefault(Return(&fake_script_executor_ui_delegate_));

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
        .WillByDefault(RunOnceCallback<2>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, GetActions)
        .WillByDefault(RunOnceCallback<5>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    ON_CALL(*mock_service_, GetNextActions)
        .WillByDefault(RunOnceCallback<6>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    ON_CALL(*mock_web_controller_, FindElement(_, _, _))
        .WillByDefault(RunOnceCallback<2>(ClientStatus(), nullptr));

    ON_CALL(mock_observer_, OnStateChanged(_))
        .WillByDefault(Invoke([this](AutofillAssistantState state) {
          states_.emplace_back(state);
        }));
    ON_CALL(mock_observer_, OnKeyboardSuppressionStateChanged(_))
        .WillByDefault(Invoke(
            [this](bool state) { keyboard_states_.emplace_back(state); }));
    controller_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    controller_->RemoveObserver(&mock_observer_);
    controller_.reset();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path,
      bool direct_action = true) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    if (direct_action) {
      script->mutable_presentation()->mutable_direct_action()->add_names(
          name_and_path);
    }
    return script;
  }

  // Defined as a function to allow access to other tests.
  void SetWebControllerForTest(Controller* controller,
                               std::unique_ptr<WebController> web_controller) {
    controller->SetWebControllerForTest(std::move(web_controller));
  }

  void SetupScripts(SupportsScriptResponseProto scripts) {
    std::string scripts_str;
    scripts.SerializeToString(&scripts_str);
    EXPECT_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, scripts_str,
                                     ServiceRequestSender::ResponseInfo{}));
  }

  void SetupActionsForScript(const std::string& path,
                             ActionsResponseProto actions_response) {
    std::string actions_response_str;
    actions_response.SerializeToString(&actions_response_str);
    EXPECT_CALL(*mock_service_, GetActions(StrEq(path), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(net::HTTP_OK, actions_response_str,
                                     ServiceRequestSender::ResponseInfo{}));
  }

  void Start() { Start("http://initialurl.com"); }

  void Start(const std::string& url_string) {
    Start(url_string, std::make_unique<TriggerContext>());
  }

  void Start(const std::string& url_string,
             std::unique_ptr<TriggerContext> trigger_context) {
    GURL url(url_string);
    SetLastCommittedUrl(url);
    controller_->Start(url, std::move(trigger_context));
  }

  void Track() {
    SetLastCommittedUrl(GURL("http://initialurl.com"));
    controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  }

  void SetLastCommittedUrl(const GURL& url) {
    content::WebContentsTester::For(web_contents())->SetLastCommittedURL(url);
  }

  void SimulateNavigateToUrl(const GURL& url) {
    SetLastCommittedUrl(url);
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        url, web_contents()->GetPrimaryMainFrame());
    content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
    controller_->DidFinishLoad(nullptr, GURL(""));
    navigation_ids_.emplace_back(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  void SimulateWebContentsFocused() {
    controller_->OnWebContentsFocused(nullptr);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                     ServiceRequestSender::ResponseInfo{}));
  }

  // Sets up all calls to the service for scripts to return |response|.
  void SetRepeatedScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
        .WillRepeatedly(RunOnceCallback<2>(
            net::HTTP_OK, response_str, ServiceRequestSender::ResponseInfo{}));
  }

  UserData* GetUserData() { return &controller_->user_data_; }

  void SetNavigatingToNewDocument(bool value) {
    controller_->navigating_to_new_document_ = value;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::TimeTicks now_;
  std::vector<AutofillAssistantState> states_;
  std::vector<bool> keyboard_states_;
  raw_ptr<MockService> mock_service_;
  raw_ptr<MockWebController> mock_web_controller_;
  NiceMock<MockClient> mock_client_;
  FakeScriptExecutorUiDelegate fake_script_executor_ui_delegate_;
  std::unique_ptr<MockRuntimeManager> mock_runtime_manager_;
  NiceMock<MockControllerObserver> mock_observer_;
  password_manager::MockPasswordChangeSuccessTracker
      mock_password_change_success_tracker_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<Controller> controller_;
  NiceMock<MockAnnotateDomModelService> mock_annotate_dom_model_service_;
  std::vector<ukm::SourceId> navigation_ids_;
};

struct NavigationState {
  bool navigating = false;
  bool has_errors = false;

  bool operator==(const NavigationState& other) const {
    return navigating == other.navigating && has_errors == other.has_errors;
  }
};

std::ostream& operator<<(std::ostream& out, const NavigationState& state) {
  out << "{navigating=" << state.navigating << ","
      << "has_errors=" << state.has_errors << "}";
  return out;
}

// A Listener that keeps track of the reported state of the delegate captured
// from OnNavigationStateChanged.
class NavigationStateChangeListener
    : public ScriptExecutorDelegate::NavigationListener {
 public:
  explicit NavigationStateChangeListener(ScriptExecutorDelegate* delegate)
      : delegate_(delegate) {}
  ~NavigationStateChangeListener() override;
  void OnNavigationStateChanged() override;

  std::vector<NavigationState> events;

 private:
  const raw_ptr<ScriptExecutorDelegate> delegate_;
};

NavigationStateChangeListener::~NavigationStateChangeListener() {}

void NavigationStateChangeListener::OnNavigationStateChanged() {
  NavigationState state;
  state.navigating = delegate_->IsNavigatingToNewDocument();
  state.has_errors = delegate_->HasNavigationError();
  events.emplace_back(state);
}

TEST_F(ControllerTest, ReportDirectActions) {
  SupportsScriptResponseProto script_response;

  AddRunnableScript(&script_response, "action");

  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  Track();

  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(),
              UnorderedElementsAre(AllOf(
                  Field(&ScriptHandle::direct_action,
                        Field(&DirectAction::names, ElementsAre("action"))))));
}

TEST_F(ControllerTest, RunDirectActionWithArguments) {
  SupportsScriptResponseProto script_response;

  // script is available as a direct action.
  auto* script1 = AddRunnableScript(&script_response, "action");
  auto* action = script1->mutable_presentation()->mutable_direct_action();
  action->add_required_arguments("required");
  action->add_optional_arguments("arg0");
  action->add_optional_arguments("arg1");

  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());

  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(),
              ElementsAre(Field(
                  &ScriptHandle::direct_action,
                  AllOf(Field(&DirectAction::names, ElementsAre("action")),
                        Field(&DirectAction::required_arguments,
                              ElementsAre("required")),
                        Field(&DirectAction::optional_arguments,
                              ElementsAre("arg0", "arg1"))))));

  EXPECT_CALL(*mock_service_, GetActions("action", _, _, _, _, _))
      .WillOnce([](const std::string& script_path, const GURL& url,
                   const TriggerContext& trigger_context,
                   const std::string& global_payload,
                   const std::string& script_payload,
                   ServiceRequestSender::ResponseCallback callback) {
        EXPECT_THAT(trigger_context.GetScriptParameters().ToProto(),
                    testing::UnorderedElementsAreArray(
                        base::flat_map<std::string, std::string>(
                            {{"required", "value"}, {"arg0", "value0"}})));
        EXPECT_TRUE(trigger_context.GetDirectAction());

        std::move(callback).Run(true, "", ServiceRequestSender::ResponseInfo{});
      });

  TriggerContext::Options options;
  options.is_direct_action = true;
  EXPECT_TRUE(controller_->PerformDirectAction(
      0, std::make_unique<TriggerContext>(
             /* parameters = */ std::make_unique<ScriptParameters>(
                 base::flat_map<std::string, std::string>{{"required", "value"},
                                                          {"arg0", "value0"}}),
             options)));
}

TEST_F(ControllerTest, NoScripts) {
  SupportsScriptResponseProto empty;
  SetNextScriptResponse(empty);

  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::NO_INITIAL_SCRIPTS));
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, NoRelevantScripts) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "no_match")
      ->mutable_presentation()
      ->mutable_precondition()
      ->add_domain("http://otherdomain.com");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::NO_INITIAL_SCRIPTS));
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, NoRelevantScriptYet) {
  SupportsScriptResponseProto script_response;
  *AddRunnableScript(&script_response, "no_match_yet")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  SetNextScriptResponse(script_response);

  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
}

TEST_F(ControllerTest, ClearDirectActionsWhenRunning) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");

  ActionsResponseProto runnable_script;
  auto* prompt_action = runnable_script.add_actions()->mutable_prompt();
  prompt_action->add_choices()->mutable_chip()->set_text("continue");

  SetupActionsForScript("script1", runnable_script);
  SetNextScriptResponse(script_response);

  Track();
  // We initially have 2 direct action scripts available.
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(2));
  // We execute one of them.
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  // There are no direct actions available once the script is running.
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(0));
}

TEST_F(ControllerTest, ScriptStartMessage) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_start_message("Starting Script...");
  SetNextScriptResponse(script_response);

  ActionsResponseProto script_actions;
  SetupActionsForScript("script", script_actions);

  Start("http://a.example.com/path");

  EXPECT_CALL(mock_observer_, OnExecuteScript("Starting Script..."));
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
}

TEST_F(ControllerTest, UpdateClientSettings) {
  SupportsScriptResponseProto script_response;
  ClientSettingsProto* initial_client_settings_proto =
      script_response.mutable_client_settings();
  initial_client_settings_proto->set_periodic_script_check_interval_ms(1);
  initial_client_settings_proto->set_display_strings_locale("en-US");
  ClientSettingsProto::DisplayString* initial_display_string;
  for (int i = 0; i < ClientSettingsProto::DisplayStringId_MAX + 1; i++) {
    initial_display_string =
        initial_client_settings_proto->add_display_strings();
    initial_display_string->set_id(
        static_cast<ClientSettingsProto::DisplayStringId>(i));
    initial_display_string->set_value("us_test");
  }
  ClientSettings initial_client_settings;
  initial_client_settings.UpdateFromProto(*initial_client_settings_proto);

  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  ClientSettingsProto* changed_client_settings_proto =
      actions_response.add_actions()
          ->mutable_update_client_settings()
          ->mutable_client_settings();
  changed_client_settings_proto->set_display_strings_locale("fr-FR");
  ClientSettingsProto::DisplayString* changed_display_string;
  for (int i = 0; i < ClientSettingsProto::DisplayStringId_MAX + 1; i++) {
    changed_display_string =
        changed_client_settings_proto->add_display_strings();
    changed_display_string->set_id(
        static_cast<ClientSettingsProto::DisplayStringId>(i));
    changed_display_string->set_value("fr_test");
  }
  ClientSettings changed_client_settings;
  changed_client_settings.UpdateFromProto(*changed_client_settings_proto);

  SetupActionsForScript("script", actions_response);

  EXPECT_CALL(mock_observer_, OnStart(_));
  testing::InSequence seq;
  EXPECT_CALL(mock_observer_,
              OnClientSettingsChanged(
                  AllOf(Field(&ClientSettings::periodic_script_check_interval,
                              base::Milliseconds(1)),
                        Field(&ClientSettings::display_strings_locale, "en-US"),
                        Field(&ClientSettings::display_strings,
                              initial_client_settings.display_strings))));
  EXPECT_CALL(mock_observer_,
              OnClientSettingsChanged(
                  AllOf(Field(&ClientSettings::periodic_script_check_interval,
                              base::Milliseconds(1)),
                        Field(&ClientSettings::display_strings_locale, "fr-FR"),
                        Field(&ClientSettings::display_strings,
                              changed_client_settings.display_strings))));
  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetSettings(),
              AllOf(Field(&ClientSettings::periodic_script_check_interval,
                          base::Milliseconds(1)),
                    Field(&ClientSettings::display_strings_locale, "fr-FR"),
                    Field(&ClientSettings::display_strings,
                          changed_client_settings.display_strings)));
}

TEST_F(ControllerTest, Shutdown) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "stop");
  SetNextScriptResponse(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, GetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, actions_response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start();
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
  // Non-gracefuls shutdowns don't call |OnStop|, we just shut down directly.
  EXPECT_CALL(mock_observer_, OnStop()).Times(0);

  testing::InSequence seq;
  EXPECT_CALL(mock_client_, Shutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN));
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
}

TEST_F(ControllerTest, ShutdownGracefully) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "stop");
  SetNextScriptResponse(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell();
  actions_response.add_actions()->mutable_stop();
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, GetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, actions_response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start();
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
  EXPECT_CALL(mock_observer_, OnStop());

  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::SCRIPT_SHUTDOWN));
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
}

TEST_F(ControllerTest, CloseCustomTab) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "stop");
  SetNextScriptResponse(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop()->set_close_cct(true);
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, GetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, actions_response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start();
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
  EXPECT_CALL(mock_observer_, CloseCustomTab());

  testing::InSequence seq;
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::CUSTOM_TAB_CLOSED));
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
}

TEST_F(ControllerTest, RefreshScriptWhenDomainChanges) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script");
  std::string scripts_str;
  script_response.SerializeToString(&scripts_str);

  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(Eq(GURL("http://a.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, scripts_str,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(Eq(GURL("http://b.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, scripts_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/path1");
  SimulateNavigateToUrl(GURL("http://a.example.com/path2"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path2"));
}

TEST_F(ControllerTest, Autostart) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto autostart_script;
  autostart_script.add_actions()->mutable_tell()->set_message("autostart");
  autostart_script.add_actions()->mutable_stop();
  SetupActionsForScript("autostart", autostart_script);

  EXPECT_CALL(mock_client_, AttachUI());
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history state transitions
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED));
  EXPECT_THAT(keyboard_states_, ElementsAre(true, true, false));
}

TEST_F(ControllerTest, InitialUrlLoads) {
  GURL initialUrl("http://a.example.com/path");
  EXPECT_CALL(*mock_service_, GetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  controller_->Start(initialUrl, std::make_unique<TriggerContext>());
}

TEST_F(ControllerTest, AttachUIWhenStarting) {
  EXPECT_CALL(mock_client_, AttachUI());
  Start();
}

TEST_F(ControllerTest, AttachUIWhenContentsFocused) {
  SimulateWebContentsFocused();  // must not call AttachUI

  testing::InSequence seq;
  EXPECT_CALL(mock_client_, AttachUI());

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  SetNextScriptResponse(script_response);
  Start();  // must call AttachUI

  EXPECT_CALL(mock_client_, AttachUI());
  SimulateWebContentsFocused();  // must call AttachUI

  EXPECT_CALL(mock_client_, AttachUI());
  controller_->OnFatalError("test", Metrics::DropOutReason::TAB_CHANGED);
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
  SimulateWebContentsFocused();  // must call AttachUI
}

TEST_F(ControllerTest, KeepCheckingForElement) {
  SupportsScriptResponseProto script_response;
  *AddRunnableScript(&script_response, "no_match_yet")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  SetNextScriptResponse(script_response);

  Track();
  // No scripts yet; the element doesn't exit.
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(0));

  for (int i = 0; i < 3; i++) {
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(0));
  }

  EXPECT_CALL(*mock_web_controller_, FindElement(_, _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
}

TEST_F(ControllerTest, ScriptTimeoutError) {
  // Wait for #element to show up for will_never_match. After 25s, execute the
  // script on_timeout_error.
  SupportsScriptResponseProto script_response;
  *AddRunnableScript(&script_response, "will_never_match")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  script_response.mutable_script_timeout_error()->set_timeout_ms(30000);
  script_response.mutable_script_timeout_error()->set_script_path(
      "on_timeout_error");
  SetNextScriptResponse(script_response);

  // on_timeout_error stops everything with a custom error message.
  ActionsResponseProto on_timeout_error;
  on_timeout_error.add_actions()->mutable_tell()->set_message("I give up");
  on_timeout_error.add_actions()->mutable_stop();
  std::string on_timeout_error_str;
  on_timeout_error.SerializeToString(&on_timeout_error_str);
  EXPECT_CALL(*mock_service_,
              GetActions(StrEq("on_timeout_error"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, on_timeout_error_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/path");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::Seconds(1));
  }
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
  EXPECT_EQ("I give up", fake_script_executor_ui_delegate_.GetStatusMessage());
}

TEST_F(ControllerTest, ScriptTimeoutWarning) {
  // Wait for #element to show up for will_never_match. After 10s, execute the
  // script on_timeout_error.
  SupportsScriptResponseProto script_response;
  *AddRunnableScript(&script_response, "will_never_match")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  script_response.mutable_script_timeout_error()->set_timeout_ms(4000);
  script_response.mutable_script_timeout_error()->set_script_path(
      "on_timeout_error");
  SetNextScriptResponse(script_response);

  // on_timeout_error displays an error message and terminates
  ActionsResponseProto on_timeout_error;
  on_timeout_error.add_actions()->mutable_tell()->set_message("This is slow");
  std::string on_timeout_error_str;
  on_timeout_error.SerializeToString(&on_timeout_error_str);
  EXPECT_CALL(*mock_service_,
              GetActions(StrEq("on_timeout_error"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, on_timeout_error_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/path");

  // Warning after 4s, script succeeds and the client continues to wait.
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::Seconds(1));
  }
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
  EXPECT_EQ("This is slow",
            fake_script_executor_ui_delegate_.GetStatusMessage());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::Seconds(1));
  }
}

TEST_F(ControllerTest, ScriptTimeoutGiveUp) {
  // Wait for #element to show up for will_never_match. After 2s, give up.
  SupportsScriptResponseProto script_response;
  ClientSettingsProto* client_settings_proto =
      script_response.mutable_client_settings();
  client_settings_proto->set_periodic_script_check_count(2);
  client_settings_proto->set_periodic_script_check_interval_ms(1000);
  *AddRunnableScript(&script_response, "will_never_match")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_, GetActions).Times(0);
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());

  // Still ok after one check / one second.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());

  // After two seconds, the last periodic check finishes and the controller
  // stops.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, SuccessfulNavigation) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());
  controller_->RemoveNavigationListener(&listener);

  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  EXPECT_THAT(listener.events, ElementsAre(NavigationState{true, false},
                                           NavigationState{false, false}));
}

TEST_F(ControllerTest, FailedNavigation) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);
  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL("http://initialurl.com"), net::ERR_CONNECTION_TIMED_OUT,
      web_contents()->GetPrimaryMainFrame());
  controller_->RemoveNavigationListener(&listener);

  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_TRUE(controller_->HasNavigationError());

  EXPECT_THAT(listener.events, ElementsAre(NavigationState{true, false},
                                           NavigationState{false, true}));
}

TEST_F(ControllerTest, NavigationWithRedirects) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://original.example.com/"),
          web_contents()->GetPrimaryMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  EXPECT_TRUE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  simulator->Redirect(GURL("http://redirect.example.com/"));
  EXPECT_TRUE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  simulator->Commit();
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  controller_->RemoveNavigationListener(&listener);

  // Redirection should not be reported as a state change.
  EXPECT_THAT(listener.events, ElementsAre(NavigationState{true, false},
                                           NavigationState{false, false}));
}

TEST_F(ControllerTest, EventuallySuccessfulNavigation) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);
  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL("http://initialurl.com"), net::ERR_CONNECTION_TIMED_OUT,
      web_contents()->GetPrimaryMainFrame());
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());
  controller_->RemoveNavigationListener(&listener);

  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  EXPECT_THAT(listener.events,
              ElementsAre(
                  // 1st navigation starts
                  NavigationState{true, false},
                  // 1st navigation fails
                  NavigationState{false, true},
                  // 2nd navigation starts, while in error state
                  NavigationState{true, true},
                  // 2nd navigation succeeds
                  NavigationState{false, false}));
}

TEST_F(ControllerTest, RemoveListener) {
  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());
  listener.events.clear();
  controller_->RemoveNavigationListener(&listener);

  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL("http://initialurl.com"), net::ERR_CONNECTION_TIMED_OUT,
      web_contents()->GetPrimaryMainFrame());
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());

  EXPECT_THAT(listener.events, IsEmpty());
}

TEST_F(ControllerTest, DelayStartupIfLoading) {
  SetNavigatingToNewDocument(true);

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::INACTIVE, controller_->GetState());
  EXPECT_EQ(controller_->GetDeeplinkURL().host(), "a.example.com");

  // Initial navigation.
  SimulateNavigateToUrl(GURL("http://b.example.com"));
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::STOPPED));
  EXPECT_EQ(controller_->GetDeeplinkURL().host(), "a.example.com");
  EXPECT_EQ(controller_->GetScriptURL().host(), "b.example.com");
  EXPECT_EQ(controller_->GetCurrentURL().host(), "b.example.com");

  // Navigation during the flow.
  SimulateNavigateToUrl(GURL("http://c.example.com"));
  EXPECT_EQ(controller_->GetDeeplinkURL().host(), "a.example.com");
  EXPECT_EQ(controller_->GetScriptURL().host(), "b.example.com");
  EXPECT_EQ(controller_->GetCurrentURL().host(), "c.example.com");
}

TEST_F(ControllerTest, WaitForNavigationActionTimesOut) {
  // A single script, with a wait_for_navigation action
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script");
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  auto* action = actions_response.add_actions()->mutable_wait_for_navigation();
  action->set_timeout_ms(1000);
  SetupActionsForScript("script", actions_response);

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Start script, which waits for some navigation event to happen after the
  // expect_navigation action has run..
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));

  // No navigation event happened within the action timeout and the script ends.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  task_environment()->FastForwardBy(base::Seconds(1));

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(TIMED_OUT, processed_actions_capture[1].status());
}

TEST_F(ControllerTest, WaitForNavigationActionStartWithinTimeout) {
  // A single script, with a wait_for_navigation action
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script");
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  auto* action = actions_response.add_actions()->mutable_wait_for_navigation();
  action->set_timeout_ms(1000);
  SetupActionsForScript("script", actions_response);

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Start script, which waits for some navigation event to happen after the
  // expect_navigation action has run..
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));

  // Navigation starts, but does not end, within the timeout.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://a.example.com/path"),
          web_contents()->GetPrimaryMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  task_environment()->FastForwardBy(base::Seconds(1));

  // Navigation finishes and the script ends.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  simulator->Commit();

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
}

TEST_F(ControllerTest, SetScriptStoreConfig) {
  // A single script, and its corresponding bundle info.
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script");
  script_response.mutable_script_store_config()->set_bundle_path("bundle/path");
  script_response.mutable_script_store_config()->set_bundle_version(12);
  SetupScripts(script_response);

  ScriptStoreConfig script_store_config;
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, SetScriptStoreConfig(_))
      .WillOnce(SaveArg<0>(&script_store_config));

  Start("http://a.example.com/path");
  controller_->GetDirectActionScripts();

  EXPECT_THAT(script_store_config.bundle_path(), Eq("bundle/path"));
  EXPECT_THAT(script_store_config.bundle_version(), Eq(12));
}

TEST_F(ControllerTest, InitialDataUrlDoesNotChange) {
  const std::string deeplink_url("http://initialurl.com/path");
  Start(deeplink_url);
  EXPECT_THAT(controller_->GetDeeplinkURL(), deeplink_url);
  EXPECT_THAT(controller_->GetCurrentURL(), deeplink_url);

  const std::string navigate_url("http://navigateurl.com/path");
  SimulateNavigateToUrl(GURL(navigate_url));
  EXPECT_THAT(controller_->GetDeeplinkURL().spec(), deeplink_url);
  EXPECT_THAT(controller_->GetCurrentURL().spec(), navigate_url);
}

TEST_F(ControllerTest, Track) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://b.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Execute the script, which requires showing the UI, then go back to tracking
  // mode
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Move to a domain for which there are no scripts. This causes the controller
  // to stop.
  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::STOPPED));
  EXPECT_THAT(keyboard_states_, ElementsAre(false, true, false, false));

  // Shutdown once we've moved from domain b.example.com, for which we know
  // there are no scripts, to c.example.com, which we don't want to check.
  EXPECT_CALL(mock_client_, Shutdown(Metrics::DropOutReason::NO_SCRIPTS));
  SimulateNavigateToUrl(GURL("http://c.example.com/"));
}

TEST_F(ControllerTest, TrackScriptWithNoUI) {
  // The UI is never shown during this test.
  EXPECT_CALL(mock_client_, AttachUI()).Times(0);

  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(false);
  SetupScripts(script_response);

  // Script does nothing
  ActionsResponseProto runnable_script;
  auto* hidden_tell = runnable_script.add_actions()->mutable_tell();
  hidden_tell->set_message("optional message");
  hidden_tell->set_needs_ui(false);
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackScriptShowUIOnTell) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(true);
  SetupScripts(script_response);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("error");
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // The last tell message should still be shown to the user.
  EXPECT_TRUE(controller_->NeedsUI());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, RunDirectActionWhileTrackingWithUi) {
  SupportsScriptResponseProto script_response;
  auto* script_needs_ui = AddRunnableScript(&script_response, "needs_ui");
  script_needs_ui->mutable_presentation()->set_needs_ui(true);

  auto* script_no_ui = AddRunnableScript(&script_response, "no_ui");
  script_no_ui->mutable_presentation()->set_needs_ui(false);
  SetupScripts(script_response);

  ActionsResponseProto needs_ui_script;
  needs_ui_script.add_actions()->mutable_tell()->set_message("error");
  needs_ui_script.add_actions()->mutable_stop();
  SetupActionsForScript("needs_ui", needs_ui_script);

  ActionsResponseProto no_ui_script;
  no_ui_script.add_actions()->mutable_stop();
  SetupActionsForScript("no_ui", no_ui_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(2));
  EXPECT_EQ(controller_->GetDirectActionScripts()[0].path, "needs_ui");

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // The last tell message should still be shown to the user.
  EXPECT_TRUE(controller_->NeedsUI());

  EXPECT_CALL(mock_client_, DestroyUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(1, std::make_unique<TriggerContext>()));

  // UI should have been cleared
  EXPECT_FALSE(controller_->NeedsUI());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackScriptClosesUI) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(false);
  SetupScripts(script_response);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("hi");
  runnable_script.add_actions()
      ->mutable_wait_for_dom()
      ->mutable_wait_condition();
  runnable_script.add_actions()->mutable_stop();

  SetupActionsForScript("runnable", runnable_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // The tell action wasn't the last one before close, so UI should close when
  // the script is finished.
  EXPECT_FALSE(controller_->NeedsUI());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackScriptShowUIOnError) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(false);
  SetupScripts(script_response);

  // Running the script fails, due to a backend issue. The error message should
  // be shown.
  EXPECT_CALL(*mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // UI must remain visible for the user to see the error message.
  EXPECT_TRUE(controller_->NeedsUI());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackContinuesAfterScriptError) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_CALL(*mock_service_, GetActions(StrEq("runnable"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  // When the script fails, the controller transitions to STOPPED state, then
  // right away back to TRACKING state.
  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackReportsFirstSetOfScripts) {
  ServiceRequestSender::ResponseCallback get_scripts_callback;
  EXPECT_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
      .WillOnce([&get_scripts_callback](
                    const GURL& url, const TriggerContext& trigger_context,
                    ServiceRequestSender::ResponseCallback callback) {
        get_scripts_callback = std::move(callback);
      });

  SetLastCommittedUrl(GURL("http://example.com/"));
  bool first_check_done = false;
  controller_->Track(std::make_unique<TriggerContext>(),
                     base::BindOnce(
                         [](Controller* controller, bool* is_done) {
                           // User actions must have been set when this is
                           // called
                           EXPECT_THAT(controller->GetDirectActionScripts(),
                                       SizeIs(1));
                           *is_done = true;
                         },
                         base::Unretained(controller_.get()),
                         base::Unretained(&first_check_done)));
  EXPECT_FALSE(first_check_done);
  EXPECT_FALSE(controller_->HasRunFirstCheck());

  ASSERT_TRUE(get_scripts_callback);

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  std::string response_str;
  script_response.SerializeToString(&response_str);
  std::move(get_scripts_callback)
      .Run(net::HTTP_OK, response_str, ServiceRequestSender::ResponseInfo{});

  EXPECT_TRUE(first_check_done);
  EXPECT_TRUE(controller_->HasRunFirstCheck());
}

TEST_F(ControllerTest, TrackReportsNoScripts) {
  SetLastCommittedUrl(GURL("http://example.com/"));
  base::MockCallback<base::OnceCallback<void()>> callback;

  EXPECT_CALL(callback, Run());
  controller_->Track(std::make_unique<TriggerContext>(), callback.Get());
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, TrackReportsNoScriptsForNow) {
  SupportsScriptResponseProto script_response;
  *AddRunnableScript(&script_response, "no_match_yet")
       ->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("#element");
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  base::MockCallback<base::OnceCallback<void()>> callback;

  EXPECT_CALL(callback, Run());
  controller_->Track(std::make_unique<TriggerContext>(), callback.Get());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
}

TEST_F(ControllerTest, TrackReportsNoScriptsForThePage) {
  // Having scripts for the domain but not for the current page is fatal in
  // STARTING or PROMPT mode, but not in TRACKING mode.
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "no_match_yet")
      ->mutable_presentation()
      ->mutable_precondition()
      ->add_path_pattern("/otherpage.html");
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  base::MockCallback<base::OnceCallback<void()>> callback;

  EXPECT_CALL(callback, Run());
  controller_->Track(std::make_unique<TriggerContext>(), callback.Get());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
}

TEST_F(ControllerTest, TrackReportsAlreadyDone) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  base::MockCallback<base::OnceCallback<void()>> callback;
  EXPECT_CALL(callback, Run());
  controller_->Track(std::make_unique<TriggerContext>(), callback.Get());
}

TEST_F(ControllerTest, TrackThenAutostart) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  AddRunnableScript(&script_response, "autostart", /*direct_action=*/false)
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  ActionsResponseProto autostart_script;
  autostart_script.add_actions()->mutable_tell()->set_message("autostart");
  autostart_script.add_actions()->mutable_stop();
  SetupActionsForScript("autostart", autostart_script);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("runnable");
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  EXPECT_CALL(mock_client_, AttachUI());
  Start("http://example.com/");
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  // Run "runnable", which then calls stop and ends. The controller should then
  // go back to TRACKING mode.
  controller_->PerformDirectAction(0, std::make_unique<TriggerContext>());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
  EXPECT_THAT(keyboard_states_,
              ElementsAre(false, true, true, false, true, false));
}

TEST_F(ControllerTest, BrowseStateStopsOnDifferentDomain) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://b.example.com/"), _, _))
      .Times(0);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://c.example.com/"), _, _))
      .Times(0);

  Start("http://example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://c.example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // go back.
  SetLastCommittedUrl(GURL("http://b.example.com"));
  content::NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // Shut down once the user moves to a different domain
  EXPECT_CALL(
      mock_client_,
      RecordDropOut(Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE));
  SimulateNavigateToUrl(GURL("http://other-example.com/"));
}

TEST_F(ControllerTest, BrowseStateWithDomainAllowlist) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  *prompt->add_browse_domains_allowlist() = "example.com";
  *prompt->add_browse_domains_allowlist() = "other-example.com";
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://sub.other-example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // go back.
  SetLastCommittedUrl(GURL("http://sub.other-example.com"));
  content::NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // Same domain navigations as one of the allowed domains should not shut down
  // AA.
  SimulateNavigateToUrl(GURL("http://other-example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // Navigation to different domain should stop AA.
  EXPECT_CALL(
      mock_client_,
      RecordDropOut(Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE));
  SimulateNavigateToUrl(GURL("http://unknown.com"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, BrowseStateWithDomainAllowlistCleanup) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  *prompt->add_browse_domains_allowlist() = "example.com";
  prompt->add_choices()->mutable_chip()->set_text("continue");

  // Second browse action without an allowlist.
  auto* prompt2 = runnable_script.add_actions()->mutable_prompt();
  prompt2->set_browse_mode(true);
  prompt2->add_choices()->mutable_chip()->set_text("done");

  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // Click "continue".
  EXPECT_EQ(
      fake_script_executor_ui_delegate_.GetUserActions()->at(0).chip().text,
      "continue");
  (*fake_script_executor_ui_delegate_.GetUserActions())[0].RunCallback();

  EXPECT_EQ(
      fake_script_executor_ui_delegate_.GetUserActions()->at(0).chip().text,
      "done");

  // Make sure the allowlist got reset with the second prompt action.
  EXPECT_CALL(
      mock_client_,
      RecordDropOut(Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE));
  SimulateNavigateToUrl(GURL("http://c.example.com/"));
}

TEST_F(ControllerTest, PromptStateStopsOnGoBack) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(false);
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://example.com/");
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://c.example.com/"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // go back.
  EXPECT_CALL(mock_client_, RecordDropOut(Metrics::DropOutReason::NAVIGATION));
  SetLastCommittedUrl(GURL("http://b.example.com"));
  content::NavigationSimulator::GoBack(web_contents());
}

TEST_F(ControllerTest, PromptStateStopsOnRendererInitiatedBack) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(false);
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://example.com/");
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://c.example.com/"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Go back, emulating a history navigation initiated from JS.
  EXPECT_CALL(mock_client_, RecordDropOut(Metrics::DropOutReason::NAVIGATION));
  SetLastCommittedUrl(GURL("http://b.example.com"));
  content::NavigationSimulator::CreateHistoryNavigation(
      -1, web_contents(), true /* is_renderer_initiated */)
      ->Commit();
}

TEST_F(ControllerTest, UnexpectedNavigationDuringPromptAction_Tracking) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  SetNextScriptResponse(script_response);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("continue");
  runnable_script.add_actions()
      ->mutable_configure_bottom_sheet()
      ->set_viewport_resizing(
          ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);
  SetupActionsForScript("runnable", runnable_script);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
  EXPECT_EQ(controller_->GetDirectActionScripts()[0].direct_action.names.count(
                "runnable"),
            1u);

  // Start the script, which should show a prompt with the continue chip.
  controller_->PerformDirectAction(0, std::make_unique<TriggerContext>());
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  ASSERT_THAT(*fake_script_executor_ui_delegate_.GetUserActions(), SizeIs(1));
  EXPECT_EQ(
      fake_script_executor_ui_delegate_.GetUserActions()->at(0).chip().text,
      "continue");

  // Browser (not document) initiated navigation while in prompt mode (such as
  // go back): The controller stops the scripts, shows an error, then goes back
  // to tracking mode.
  //
  // The ConfigureBottomSheet action which follows the prompt should never be
  // executed.
  EXPECT_CALL(mock_observer_, OnViewportModeChanged(_)).Times(0);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/otherpage"));

  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));
  EXPECT_EQ(controller_->GetDirectActionScripts()[0].direct_action.names.count(
                "runnable"),
            1u);

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, UnexpectedNavigationDuringPromptAction) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto autostart_script;
  autostart_script.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("continue");
  autostart_script.add_actions()
      ->mutable_configure_bottom_sheet()
      ->set_viewport_resizing(
          ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  ASSERT_THAT(*fake_script_executor_ui_delegate_.GetUserActions(), SizeIs(1));
  EXPECT_EQ(
      fake_script_executor_ui_delegate_.GetUserActions()->at(0).chip().text,
      "continue");

  // Browser (not document) initiated navigation while in prompt mode (such as
  // go back): The controller stops the scripts, shows an error and shuts down.
  //
  // The ConfigureBottomSheet action which follows the prompt should never be
  // executed.
  EXPECT_CALL(mock_observer_, OnViewportModeChanged(_)).Times(0);

  // Renderer (Document) initiated navigation is allowed.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://a.example.com/page"), web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Expected browser initiated navigation is allowed.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  controller_->ExpectNavigation();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://b.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Unexpected browser initiated navigation will cause an error.
  EXPECT_CALL(mock_client_, RecordDropOut(Metrics::DropOutReason::NAVIGATION));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://c.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, UnexpectedNavigationInRunningState) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto autostart_script;
  auto* wait_for_dom = autostart_script.add_actions()->mutable_wait_for_dom();
  wait_for_dom->set_timeout_ms(10000);
  wait_for_dom->mutable_wait_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("#some-element");
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::RUNNING, controller_->GetState());

  // Document (not user) initiated navigation while in RUNNING state:
  // The controller keeps going.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://a.example.com/page"), web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(AutofillAssistantState::RUNNING, controller_->GetState());

  // Expected browser initiated navigation while in RUNNING state:
  // The controller keeps going.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  controller_->ExpectNavigation();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://b.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::RUNNING, controller_->GetState());

  // Unexpected browser initiated navigation while in RUNNING state:
  // The controller stops the scripts, shows an error and shuts down.
  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING));
  EXPECT_CALL(mock_observer_,
              OnError(_, Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://c.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, NavigationAfterStopped) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto autostart_script;
  autostart_script.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("continue");
  std::string never_shown = "never shown";
  autostart_script.add_actions()->mutable_tell()->set_message(never_shown);
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Unexpected browser initiated navigation will cause an error.
  EXPECT_CALL(mock_client_, RecordDropOut(Metrics::DropOutReason::NAVIGATION));
  EXPECT_CALL(mock_observer_, OnError(_, Metrics::DropOutReason::NAVIGATION));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://a.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Another navigation will destroy the UI.
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://b.example.com/page"));

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, NavigationWhileTrackingWithUi) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(true);
  SetupScripts(script_response);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("error");
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(std::make_unique<TriggerContext>(), base::DoNothing());
  ASSERT_THAT(controller_->GetDirectActionScripts(), SizeIs(1));

  EXPECT_TRUE(
      controller_->PerformDirectAction(0, std::make_unique<TriggerContext>()));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_TRUE(controller_->NeedsUI());

  // Browser navigation will destroy the UI.
  EXPECT_CALL(mock_client_, DestroyUISoon());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://a.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_FALSE(controller_->NeedsUI());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, NavigationToGooglePropertyShutsDownDestroyingUI) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto autostart_script;
  autostart_script.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("continue");
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  EXPECT_CALL(mock_client_, Shutdown(Metrics::DropOutReason::NAVIGATION));
  GURL google("https://google.com/search");
  SetLastCommittedUrl(google);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             google);

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));
}

TEST_F(ControllerTest,
       DomainChangeToGooglePropertyDuringBrowseShutsDownDestroyingUI) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              GetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, response_str,
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  EXPECT_CALL(
      mock_client_,
      Shutdown(Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE));
  GURL google("https://google.com/search");
  SetLastCommittedUrl(google);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             google);

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::BROWSE));
}

TEST_F(ControllerTest, SetOverlayColors) {
  EXPECT_CALL(
      mock_observer_,
      OnOverlayColorsChanged(AllOf(
          Field(&Controller::OverlayColors::background, StrEq("#FF000000")),
          Field(&Controller::OverlayColors::highlight_border,
                StrEq("#FFFFFFFF")))));

  GURL url("http://a.example.com/path");
  controller_->Start(url,
                     std::make_unique<TriggerContext>(
                         /* parameters = */ std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"OVERLAY_COLORS", "#FF000000:#FFFFFFFF"}}),
                         TriggerContext::Options()));
}

TEST_F(ControllerTest, AddParametersToUserData) {
  auto script_parameters = std::make_unique<ScriptParameters>(
      base::flat_map<std::string, std::string>{{"PARAM_A", "a"}});
  script_parameters->UpdateDeviceOnlyParameters(
      base::flat_map<std::string, std::string>{{"PARAM_B", "b"}});
  GURL url("http://a.example.com/path");
  controller_->Start(
      url, std::make_unique<TriggerContext>(std::move(script_parameters),
                                            TriggerContext::Options()));

  EXPECT_EQ(controller_->GetUserData()
                ->GetAdditionalValue("param:PARAM_A")
                ->strings()
                .values(0),
            "a");
  EXPECT_FALSE(controller_->GetUserData()
                   ->GetAdditionalValue("param:PARAM_A")
                   ->is_client_side_only());
  EXPECT_EQ(controller_->GetUserData()
                ->GetAdditionalValue("param:PARAM_B")
                ->strings()
                .values(0),
            "b");
  EXPECT_TRUE(controller_->GetUserData()
                  ->GetAdditionalValue("param:PARAM_B")
                  ->is_client_side_only());
}

TEST_F(ControllerTest, WriteUserData) {
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(_, UserDataFieldChange::TERMS_AND_CONDITIONS));

  base::OnceCallback<void(UserData*, UserDataFieldChange*)> callback =
      base::BindOnce([](UserData* data, UserDataFieldChange* change) {
        data->terms_and_conditions_ = TermsAndConditionsState::ACCEPTED;
        *change = UserDataFieldChange::TERMS_AND_CONDITIONS;
      });

  controller_->WriteUserData(std::move(callback));
  EXPECT_EQ(GetUserData()->terms_and_conditions_,
            TermsAndConditionsState::ACCEPTED);
}

TEST_F(ControllerTest, StartPasswordChangeFlowFromUPM) {
  const GURL initialUrl("http://example.com/password");
  const std::string username = "test_username";
  EXPECT_CALL(*mock_service_, GetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_password_change_success_tracker_,
              OnChangePasswordFlowStarted(
                  initialUrl.DeprecatedGetOriginAsURL(), username,
                  password_manager::PasswordChangeSuccessTracker::StartEvent::
                      kAutomatedFlow,
                  password_manager::PasswordChangeSuccessTracker::EntryPoint::
                      kLeakCheckInSettings));

  EXPECT_TRUE(controller_->Start(
      initialUrl,
      // 9 is the enum value of |GOOGLE_PASSWORD_MANAGER|, i.e. a call
      // from the Unified Password Manager.
      std::make_unique<TriggerContext>(
          /* parameters=*/std::make_unique<ScriptParameters>(
              base::flat_map<std::string, std::string>{
                  {"PASSWORD_CHANGE_USERNAME", username}, {"CALLER", "9"}}),
          TriggerContext::Options())));
  // Initial navigation.
  SimulateNavigateToUrl(GURL("http://b.example.com"));
  EXPECT_EQ(GetUserData()->selected_login_->username, username);
  EXPECT_EQ(GetUserData()->selected_login_->origin,
            initialUrl.DeprecatedGetOriginAsURL());
  EXPECT_EQ(controller_->GetCurrentURL().host(), "b.example.com");
}

TEST_F(ControllerTest, StartPasswordChangeFlow) {
  const GURL initialUrl("http://example.com/password");
  const std::string username = "test_username";
  EXPECT_CALL(*mock_service_, GetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));
  // We do not expect a call to the tracker, since the flow is not started
  // from UPM.
  EXPECT_CALL(mock_password_change_success_tracker_,
              OnChangePasswordFlowStarted)
      .Times(0);

  EXPECT_TRUE(controller_->Start(
      initialUrl, std::make_unique<TriggerContext>(
                      /* parameters = */ std::make_unique<ScriptParameters>(
                          base::flat_map<std::string, std::string>{
                              {"PASSWORD_CHANGE_USERNAME", username}}),
                      TriggerContext::Options())));
  // Initial navigation.
  SimulateNavigateToUrl(GURL("http://b.example.com"));
  EXPECT_EQ(GetUserData()->selected_login_->username, username);
  EXPECT_EQ(GetUserData()->selected_login_->origin,
            initialUrl.DeprecatedGetOriginAsURL());
  EXPECT_EQ(controller_->GetCurrentURL().host(), "b.example.com");
}

TEST_F(ControllerTest, EndPromptWithOnEndNavigation) {
  // A single script, with a prompt action and on_end_navigation enabled.
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  auto* action = actions_response.add_actions()->mutable_prompt();
  action->set_end_on_navigation(true);
  action->add_choices()->mutable_chip()->set_text("ok");

  actions_response.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("ok 2");

  SetupActionsForScript("script", actions_response);

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  Start("http://a.example.com/path");

  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(*fake_script_executor_ui_delegate_.GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("ok")))));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://a.example.com/path"),
          web_contents()->GetPrimaryMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  task_environment()->FastForwardBy(base::Seconds(1));

  // Commit the navigation, which will end the current prompt.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  simulator->Commit();

  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(*fake_script_executor_ui_delegate_.GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("ok 2")))));

  (*fake_script_executor_ui_delegate_.GetUserActions())[0].RunCallback();

  EXPECT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
  EXPECT_TRUE(processed_actions_capture[0].prompt_choice().navigation_ended());
  EXPECT_FALSE(processed_actions_capture[1].prompt_choice().navigation_ended());
}

TEST_F(ControllerTest, CallingShutdownIfNecessaryShutsDownTheFlow) {
  SupportsScriptResponseProto empty;
  SetNextScriptResponse(empty);

  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::NO_INITIAL_SCRIPTS));
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Note that even if we expect Shutdown to be called with
  // UI_CLOSED_UNEXPECTEDLY, the reported reason in this case would be
  // NO_INITIAL_SCRIPTS since the reason passed as argument in Shutdown is
  // ignore if another reason has been previously reported.
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY));
  controller_->ShutdownIfNecessary();
}

TEST_F(ControllerTest, ShutdownDirectlyWhenNeverHadUi) {
  SupportsScriptResponseProto empty;
  SetNextScriptResponse(empty);

  EXPECT_CALL(mock_client_, HasHadUI()).WillOnce(Return(false));
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::NO_INITIAL_SCRIPTS));
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, RegularScriptNotifiesStart) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  script->mutable_presentation()->set_start_message("start message");
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello World");

  SetupActionsForScript("script", actions_response);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnStart(_));
  EXPECT_CALL(mock_observer_, OnExecuteScript("start message"));

  Start("http://a.example.com/path");
}

TEST_F(ControllerTest, NotifyRuntimeManagerOnUiStateChange) {
  EXPECT_CALL(*mock_runtime_manager_, SetUIState(UIState::kShown));
  controller_->SetUiShown(true);

  EXPECT_CALL(*mock_runtime_manager_, SetUIState(UIState::kNotShown));
  controller_->SetUiShown(false);
}

TEST_F(ControllerTest, RuntimeManagerDestroyed) {
  mock_runtime_manager_.reset();
  // This method should not crash.
  controller_->SetUiShown(true);
}

TEST_F(ControllerTest, OnGetScriptsFailedWillShutdown) {
  EXPECT_CALL(mock_observer_, OnStart(_));
  EXPECT_CALL(*mock_service_, GetScriptsForUrl(_, _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_NOT_FOUND, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(
      mock_observer_,
      OnError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
              Metrics::DropOutReason::GET_SCRIPTS_FAILED));
  EXPECT_CALL(mock_client_, HasHadUI()).WillOnce(Return(false));
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::GET_SCRIPTS_FAILED));

  Start();
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
}

TEST_F(ControllerTest, FlowFinishedMetricOneRoundtripNoActions) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  EXPECT_CALL(*mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_OK, "",
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 13}));

  Start("http://a.example.com/path");

  EXPECT_THAT(
      GetUkmFlowFinished(ukm_recorder_),
      ElementsAreArray(std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>{
          {navigation_ids_[0],
           {{kFlowFinishedState, 1 /*Metrics::FlowFinishedState::SUCCESS*/},
            {kFlowFinishedNumActions, 0},
            {kFlowFinishedNumJsFlowActions, 0},
            {kFlowFinishedNumRoundtrips, 1},
            {kFlowFinishedTotalDecodedGetActionsSizeInBytes, 0},
            {kFlowFinishedTotalDecodedJsFlowSizeInBytes, 0},
            {kFlowFinishedTotalEncodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(13)}}}}));
}

TEST_F(ControllerTest, FlowFinishedMetricFailedRoundtrip) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  EXPECT_CALL(*mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_UNAUTHORIZED, "",
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 13}));

  Start("http://a.example.com/path");

  EXPECT_THAT(
      GetUkmFlowFinished(ukm_recorder_),
      ElementsAreArray(std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>{
          {navigation_ids_[0],
           {{kFlowFinishedState, 2 /*Metrics::FlowFinishedState::FAILURE*/},
            {kFlowFinishedNumActions, 0},
            {kFlowFinishedNumJsFlowActions, 0},
            {kFlowFinishedNumRoundtrips, 1},
            {kFlowFinishedTotalDecodedGetActionsSizeInBytes, 0},
            {kFlowFinishedTotalDecodedJsFlowSizeInBytes, 0},
            {kFlowFinishedTotalEncodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(13)}}}}));
}

TEST_F(ControllerTest, FlowFinishedMetricControllerDestroyedMidFlow) {
  auto service = std::make_unique<NiceMock<MockService>>();
  auto* service_ptr = service.get();

  auto controller = std::make_unique<Controller>(
      web_contents(), &mock_client_, task_environment()->GetMockTickClock(),
      mock_runtime_manager_->GetWeakPtr(), std::move(service), &ukm_recorder_,
      /* annotate_dom_model_service= */ nullptr);
  SetWebControllerForTest(controller.get(),
                          std::make_unique<NiceMock<MockWebController>>());

  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  std::string serialized_script_response;
  script_response.SerializeToString(&serialized_script_response);
  EXPECT_CALL(*service_ptr, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_script_response,
                                   ServiceRequestSender::ResponseInfo{}));

  // Run a prompt to block execution.
  ActionsResponseProto roundtrip;
  roundtrip.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("ok");
  std::string serialized_roundtrip;
  roundtrip.SerializeToString(&serialized_roundtrip);
  EXPECT_CALL(*service_ptr, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_OK, serialized_roundtrip,
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 10}));
  controller->Start(GURL("http://a.example.com/path"),
                    std::make_unique<TriggerContext>());
  EXPECT_THAT(GetUkmFlowFinished(ukm_recorder_), IsEmpty());

  controller.reset();
  EXPECT_THAT(
      GetUkmFlowFinished(ukm_recorder_),
      ElementsAreArray(std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>{
          {navigation_ids_[0],
           {{kFlowFinishedState, 3 /*Metrics::FlowFinishedState::DESTROYED*/},
            {kFlowFinishedNumActions, 1},
            {kFlowFinishedNumJsFlowActions, 0},
            {kFlowFinishedNumRoundtrips, 1},
            {kFlowFinishedTotalDecodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(serialized_roundtrip.size())},
            {kFlowFinishedTotalDecodedJsFlowSizeInBytes, 0},
            {kFlowFinishedTotalEncodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(10)}}}}));
}

TEST_F(ControllerTest, FlowFinishedMetricMultipleRoundtrips) {
  // A single script with a prompt action.
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  // First roundtrip, containing a single action and pretending to be 10 bytes.
  ActionsResponseProto first_roundtrip;
  first_roundtrip.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("ok 1");
  std::string serialized_first_roundtrip;
  first_roundtrip.SerializeToString(&serialized_first_roundtrip);
  EXPECT_CALL(*mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_OK, serialized_first_roundtrip,
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 10}));

  // Second roundtrip, containing three actions (one of which a JS flow action),
  // pretending to be 15 bytes long. Note that the first action fails
  // immediately, leading to only a subset of actions being executed (but they
  // should all count towards network traffic).
  ActionsResponseProto second_roundtrip;
  // Invalid showcast, since no selector is specified.
  second_roundtrip.add_actions()->mutable_show_cast();
  second_roundtrip.add_actions()->mutable_js_flow()->set_js_flow("return 5;");
  second_roundtrip.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("ok 2");
  std::string serialized_second_roundtrip;
  second_roundtrip.SerializeToString(&serialized_second_roundtrip);
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(
          net::HTTP_OK, serialized_second_roundtrip,
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 15}))
      // Third roundtrip is empty, ending the flow.
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  Start("http://a.example.com/path");
  EXPECT_THAT(*fake_script_executor_ui_delegate_.GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("ok 1")))));
  (*fake_script_executor_ui_delegate_.GetUserActions())[0].RunCallback();

  EXPECT_THAT(
      GetUkmFlowFinished(ukm_recorder_),
      ElementsAreArray(std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>{
          {navigation_ids_[0],
           {{kFlowFinishedState, 1 /*Metrics::FlowFinishedState::SUCCESS*/},
            {kFlowFinishedNumActions, 4},
            {kFlowFinishedNumJsFlowActions, 1},
            {kFlowFinishedNumRoundtrips, 3},
            {kFlowFinishedTotalDecodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(
                 serialized_first_roundtrip.size() +
                 serialized_second_roundtrip.size())},
            {kFlowFinishedTotalDecodedJsFlowSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(
                 second_roundtrip.actions(1).SerializeAsString().size())},
            {kFlowFinishedTotalEncodedGetActionsSizeInBytes,
             ukm::GetExponentialBucketMinForBytes(
                 25 /* sum of response infos */)}}}}));
}

class ControllerPrerenderTest : public ControllerTest {
 public:
  ControllerPrerenderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }

  ~ControllerPrerenderTest() override = default;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ControllerPrerenderTest, SuccessfulNavigation) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());

  EXPECT_THAT(
      listener.events,
      ElementsAre(
          NavigationState{/* navigating= */ true, /* has_errors= */ false},
          NavigationState{/* navigating= */ false, /* has_errors= */ false}));

  listener.events.clear();

  // Start prerendering a page.
  const GURL prerendering_url("http://initialurl.com?prerendering");
  auto simulator = content::WebContentsTester::For(web_contents())
                       ->AddPrerenderAndStartNavigation(prerendering_url);
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  simulator->Commit();
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  controller_->RemoveNavigationListener(&listener);

  EXPECT_THAT(listener.events, IsEmpty());
}

TEST_F(ControllerTest, MustUseBackendData) {
  EXPECT_CALL(mock_client_, MustUseBackendData).WillOnce(Return(true));
  EXPECT_TRUE(controller_->MustUseBackendData());
}

class ControllerFencedFrameTest : public ControllerTest {
 public:
  ControllerFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~ControllerFencedFrameTest() override = default;

  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* fenced_frame =
        content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ControllerFencedFrameTest, DoNotNavigateInFencedFrame) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetPrimaryMainFrame());

  EXPECT_THAT(
      listener.events,
      ElementsAre(
          NavigationState{/* navigating= */ true, /* has_errors= */ false},
          NavigationState{/* navigating= */ false, /* has_errors= */ false}));

  listener.events.clear();

  // Create a fenced frame.
  content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(web_contents()->GetPrimaryMainFrame());
  GURL kFencedFrameUrl("https://fencedframe.com");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(kFencedFrameUrl,
                                                            fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = navigation_simulator->GetFinalRenderFrameHost();
  EXPECT_TRUE(fenced_frame_rfh->IsFencedFrameRoot());

  // Autofill assistant controller doesn't handle navigations in fenced frames.
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  controller_->RemoveNavigationListener(&listener);

  EXPECT_THAT(listener.events, IsEmpty());
}

TEST_F(ControllerTest, SemanticOverridesSetInService) {
  EXPECT_CALL(mock_annotate_dom_model_service_, SetOverridesPolicy)
      .WillOnce(Return(true));

  SupportsScriptResponseProto script_response;
  script_response.mutable_semantic_selector_policy()
      ->mutable_bag_of_words()
      ->add_data_point_map();
  AddRunnableScript(&script_response, "runnable");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(mock_client_, AttachUI());
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
}

TEST_F(ControllerTest, SkipModelVersionIfParameterNotSpecified) {
  EXPECT_CALL(mock_client_, GetAnnotateDomModelVersion).Times(0);
  EXPECT_CALL(*mock_service_, UpdateAnnotateDomModelContext).Times(0);
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  controller_->Start(GURL("https://www.example.com"),
                     std::make_unique<TriggerContext>(
                         /* parameters = */ std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{{}}),
                         TriggerContext::Options()));
}

TEST_F(ControllerTest, AttachesAvailableModelVersionOnStart) {
  EXPECT_CALL(mock_client_, GetAnnotateDomModelVersion)
      .WillOnce(RunOnceCallback<0>(123456));
  EXPECT_CALL(*mock_service_, UpdateAnnotateDomModelContext(123456));
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  controller_->Start(GURL("https://www.example.com"),
                     std::make_unique<TriggerContext>(
                         /* parameters = */ std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"SEND_ANNOTATE_DOM_MODEL_VERSION", "true"}}),
                         TriggerContext::Options()));
}

TEST_F(ControllerTest, DoesNotAttachUnavailableModelVersionOnStart) {
  EXPECT_CALL(mock_client_, GetAnnotateDomModelVersion)
      .WillOnce(RunOnceCallback<0>(absl::nullopt));
  EXPECT_CALL(*mock_service_, UpdateAnnotateDomModelContext).Times(0);
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  controller_->Start(GURL("https://www.example.com"),
                     std::make_unique<TriggerContext>(
                         /* parameters = */ std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{
                                 {"SEND_ANNOTATE_DOM_MODEL_VERSION", "true"}}),
                         TriggerContext::Options()));
}

TEST_F(ControllerTest, AttachesAvailableModelVersionForCommandLineSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantAnnotateDom, "true");

  EXPECT_CALL(mock_client_, GetAnnotateDomModelVersion)
      .WillOnce(RunOnceCallback<0>(123456));
  EXPECT_CALL(*mock_service_, UpdateAnnotateDomModelContext(123456));
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  controller_->Start(GURL("https://www.example.com"),
                     std::make_unique<TriggerContext>(
                         /* parameters = */ std::make_unique<ScriptParameters>(
                             base::flat_map<std::string, std::string>{}),
                         TriggerContext::Options()));
}

TEST_F(ControllerTest, UpdatesJsFlowLibraryLoaded) {
  EXPECT_CALL(*mock_service_, UpdateJsFlowLibraryLoaded(true));

  controller_->SetJsFlowLibrary("const st = 2;");
}

TEST_F(ControllerTest, JsFlowLibraryNotLoadedForEmpty) {
  EXPECT_CALL(*mock_service_, UpdateJsFlowLibraryLoaded(true)).Times(0);

  controller_->SetJsFlowLibrary("");
}

}  // namespace autofill_assistant
