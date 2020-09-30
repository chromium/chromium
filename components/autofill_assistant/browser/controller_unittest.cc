// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_controller_observer.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {

// Same as non-mock, but provides default mock callbacks.
struct MockCollectUserDataOptions : public CollectUserDataOptions {
  MockCollectUserDataOptions() {
    base::MockOnceCallback<void(UserData*, const UserModel*)>
        mock_confirm_callback;
    confirm_callback = mock_confirm_callback.Get();
    base::MockOnceCallback<void(int, UserData*, const UserModel*)>
        mock_actions_callback;
    additional_actions_callback = mock_actions_callback.Get();
    base::MockOnceCallback<void(int, UserData*, const UserModel*)>
        mock_terms_callback;
    terms_link_callback = mock_terms_callback.Get();
  }
};

}  // namespace

class ControllerTest : public content::RenderViewHostTestHarness {
 public:
  ControllerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ControllerTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillAssistantChromeEntry);
    auto web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = web_controller.get();
    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();

    ON_CALL(mock_client_, GetWebContents).WillByDefault(Return(web_contents()));
    ON_CALL(mock_client_, HasHadUI()).WillByDefault(Return(true));

    controller_ = std::make_unique<Controller>(
        web_contents(), &mock_client_, task_environment()->GetMockTickClock(),
        &mock_runtime_manager_, std::move(service));
    controller_->SetWebControllerForTest(std::move(web_controller));

    ON_CALL(mock_client_, AttachUI()).WillByDefault(Invoke([this]() {
      controller_->SetUiShown(true);
    }));

    ON_CALL(mock_client_, DestroyUI()).WillByDefault(Invoke([this]() {
      controller_->SetUiShown(false);
    }));

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, ""));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, OnGetActions(_, _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(true, ""));

    ON_CALL(*mock_service_, OnGetNextActions(_, _, _, _, _))
        .WillByDefault(RunOnceCallback<4>(true, ""));

    ON_CALL(*mock_service_, IsLiteService).WillByDefault(Return(false));

    ON_CALL(*mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));

    ON_CALL(mock_observer_, OnStateChanged(_))
        .WillByDefault(Invoke([this](AutofillAssistantState state) {
          states_.emplace_back(state);
        }));
    controller_->AddObserver(&mock_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&mock_observer_); }

 protected:
  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    script->mutable_presentation()->mutable_chip()->set_text(name_and_path);
    return script;
  }

  static void RunOnce(SupportedScriptProto* proto) {
    auto* run_once = proto->mutable_presentation()
                         ->mutable_precondition()
                         ->add_script_status_match();
    run_once->set_script(proto->path());
    run_once->set_status(SCRIPT_STATUS_NOT_RUN);
  }

  void SetupScripts(SupportsScriptResponseProto scripts) {
    std::string scripts_str;
    scripts.SerializeToString(&scripts_str);
    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, scripts_str));
  }

  void SetupActionsForScript(const std::string& path,
                             ActionsResponseProto actions_response) {
    std::string actions_response_str;
    actions_response.SerializeToString(&actions_response_str);
    EXPECT_CALL(*mock_service_, OnGetActions(StrEq(path), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(true, actions_response_str));
  }

  void Start() { Start("http://initialurl.com"); }

  void Start(const std::string& url_string) {
    Start(url_string, TriggerContext::CreateEmpty());
  }

  void Start(const std::string& url_string,
             std::unique_ptr<TriggerContext> trigger_context) {
    GURL url(url_string);
    SetLastCommittedUrl(url);
    controller_->Start(url, std::move(trigger_context));
  }

  void SetLastCommittedUrl(const GURL& url) {
    content::WebContentsTester::For(web_contents())->SetLastCommittedURL(url);
  }

  void SimulateNavigateToUrl(const GURL& url) {
    SetLastCommittedUrl(url);
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        url, web_contents()->GetMainFrame());
    content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
    controller_->DidFinishLoad(nullptr, GURL(""));
  }

  void SimulateWebContentsFocused() {
    controller_->OnWebContentsFocused(nullptr);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, response_str));
  }

  // Sets up all calls to the service for scripts to return |response|.
  void SetRepeatedScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillRepeatedly(RunOnceCallback<2>(true, response_str));
  }

  UserData* GetUserData() { return controller_->user_data_.get(); }

  UiDelegate* GetUiDelegate() { return controller_.get(); }

  void SetNavigatingToNewDocument(bool value) {
    controller_->navigating_to_new_document_ = value;
  }

  // |task_environment_| must be the first field, to make sure that everything
  // runs in the same task environment.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::TimeTicks now_;
  std::vector<AutofillAssistantState> states_;
  MockService* mock_service_;
  MockWebController* mock_web_controller_;
  NiceMock<MockClient> mock_client_;
  NiceMock<MockRuntimeManager> mock_runtime_manager_;
  NiceMock<MockControllerObserver> mock_observer_;
  std::unique_ptr<Controller> controller_;
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
  ScriptExecutorDelegate* const delegate_;
};

NavigationStateChangeListener::~NavigationStateChangeListener() {}

void NavigationStateChangeListener::OnNavigationStateChanged() {
  NavigationState state;
  state.navigating = delegate_->IsNavigatingToNewDocument();
  state.has_errors = delegate_->HasNavigationError();
  events.emplace_back(state);
}

class ScriptExecutorListener : public ScriptExecutorDelegate::Listener {
 public:
  explicit ScriptExecutorListener() = default;
  ~ScriptExecutorListener() override;

  void OnPause(const std::string& message,
               const std::string& button_label) override;

  int pause_count = 0;
};

ScriptExecutorListener::~ScriptExecutorListener() {}

void ScriptExecutorListener::OnPause(const std::string& message,
                                     const std::string& button_label) {
  ++pause_count;
}

TEST_F(ControllerTest, FetchAndRunScriptsWithChip) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  auto* script2 = AddRunnableScript(&script_response, "script2");
  RunOnce(script2);
  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  Start("http://a.example.com/path");

  // Offering the choices: script1 and script2
  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
  EXPECT_THAT(
      controller_->GetUserActions(),
      UnorderedElementsAre(Property(&UserAction::chip,
                                    AllOf(Field(&Chip::text, StrEq("script1")),
                                          Field(&Chip::type, NORMAL_ACTION))),
                           Property(&UserAction::chip,
                                    AllOf(Field(&Chip::text, StrEq("script2")),
                                          Field(&Chip::type, NORMAL_ACTION)))));

  // Choose script2 and run it successfully.
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("script2"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));
  EXPECT_TRUE(controller_->PerformUserAction(1));

  // Offering the remaining choice: script1 as script2 can only run once.
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("script1")))));
}

TEST_F(ControllerTest, ReportDirectActions) {
  SupportsScriptResponseProto script_response;

  // script1 is available as a chip and a direct action.
  auto* script1 = AddRunnableScript(&script_response, "script1");
  script1->mutable_presentation()->mutable_direct_action()->add_names(
      "action_1");

  // script1 is available only as a direct action.
  auto* script2 = AddRunnableScript(&script_response, "script2");
  script2->mutable_presentation()->mutable_direct_action()->add_names(
      "action_2");
  script2->mutable_presentation()->clear_chip();

  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  Start("http://a.example.com/path");

  // Offering the choices: script1 and script2
  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
  EXPECT_THAT(
      controller_->GetUserActions(),
      UnorderedElementsAre(
          AllOf(Property(&UserAction::chip, Field(&Chip::text, "script1")),
                Property(&UserAction::direct_action,
                         Field(&DirectAction::names, ElementsAre("action_1")))),
          AllOf(
              Property(&UserAction::chip, Property(&Chip::empty, true)),
              Property(&UserAction::direct_action,
                       Field(&DirectAction::names, ElementsAre("action_2"))))));
}

TEST_F(ControllerTest, RunDirectActionWithArguments) {
  SupportsScriptResponseProto script_response;

  // script is available as a chip and a direct action.
  auto* script1 = AddRunnableScript(&script_response, "script");
  auto* action = script1->mutable_presentation()->mutable_direct_action();
  action->add_names("action");
  action->add_required_arguments("required");
  action->add_optional_arguments("arg0");
  action->add_optional_arguments("arg1");

  SetNextScriptResponse(script_response);

  testing::InSequence seq;

  Start("http://a.example.com/path");

  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(
                  &UserAction::direct_action,
                  AllOf(Field(&DirectAction::names, ElementsAre("action")),
                        Field(&DirectAction::required_arguments,
                              ElementsAre("required")),
                        Field(&DirectAction::optional_arguments,
                              ElementsAre("arg0", "arg1"))))));

  EXPECT_CALL(*mock_service_, OnGetActions("script", _, _, _, _, _))
      .WillOnce(Invoke([](const std::string& script_path, const GURL& url,
                          const TriggerContext& trigger_context,
                          const std::string& global_payload,
                          const std::string& script_payload,
                          Service::ResponseCallback& callback) {
        EXPECT_THAT("value",
                    trigger_context.GetParameter("required").value_or(""));
        EXPECT_THAT("value0",
                    trigger_context.GetParameter("arg0").value_or(""));

        std::move(callback).Run(true, "");
      }));

  std::map<std::string, std::string> parameters;
  parameters["required"] = "value";
  parameters["arg0"] = "value0";
  EXPECT_TRUE(controller_->PerformUserActionWithContext(
      0, TriggerContext::Create(parameters, "")));
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

TEST_F(ControllerTest, ReportPromptAndActionsChanged) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(2)));
  Start("http://a.example.com/path");

  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
}

TEST_F(ControllerTest, ClearUserActionsWhenRunning) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  // Discover 2 scripts, one is selected and run (with no chips shown), then the
  // same chips are shown.
  {
    testing::InSequence seq;
    // Discover 2 scripts, script1 and script2.
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(2)));
    // Set of chips is cleared while running script1.
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
    // This test doesn't specify what happens after that.
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(_)).Times(AnyNumber());
  }
  Start("http://a.example.com/path");
  EXPECT_TRUE(controller_->PerformUserAction(0));
}

TEST_F(ControllerTest, ShowFirstInitialStatusMessage) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");

  SupportedScriptProto* script2 =
      AddRunnableScript(&script_response, "script2");
  script2->mutable_presentation()->set_initial_prompt("script2 prompt");
  script2->mutable_presentation()->set_priority(10);

  SupportedScriptProto* script3 =
      AddRunnableScript(&script_response, "script3");
  script3->mutable_presentation()->set_initial_prompt("script3 prompt");
  script3->mutable_presentation()->set_priority(5);

  SupportedScriptProto* script4 =
      AddRunnableScript(&script_response, "script4");
  script4->mutable_presentation()->set_initial_prompt("script4 prompt");
  script4->mutable_presentation()->set_priority(8);

  SetNextScriptResponse(script_response);

  Start("http://a.example.com/path");

  EXPECT_THAT(controller_->GetUserActions(), SizeIs(4));
  // Script3, with higher priority (lower number), wins.
  EXPECT_EQ("script3 prompt", controller_->GetStatusMessage());
}

TEST_F(ControllerTest, ScriptStartMessage) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_start_message("Starting Script...");
  SetNextScriptResponse(script_response);

  ActionsResponseProto script_actions;
  script_actions.add_actions()->mutable_tell()->set_message("Script running.");
  SetupActionsForScript("script", script_actions);

  Start("http://a.example.com/path");

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Starting Script..."));
    EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Script running."));
  }
  EXPECT_TRUE(controller_->PerformUserAction(0));
}

TEST_F(ControllerTest, Stop) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "stop");
  SetNextScriptResponse(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, actions_response_str));

  Start();
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  testing::InSequence seq;
  EXPECT_CALL(mock_client_, Shutdown(Metrics::DropOutReason::SCRIPT_SHUTDOWN));
  EXPECT_TRUE(controller_->PerformUserAction(0));
}

TEST_F(ControllerTest, CloseCustomTab) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "stop");
  SetNextScriptResponse(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop()->set_close_cct(true);
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("stop"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, actions_response_str));

  Start();
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_CALL(mock_observer_, CloseCustomTab()).Times(1);

  testing::InSequence seq;
  EXPECT_CALL(mock_client_,
              Shutdown(Metrics::DropOutReason::CUSTOM_TAB_CLOSED));
  EXPECT_TRUE(controller_->PerformUserAction(0));
}

TEST_F(ControllerTest, RefreshScriptWhenDomainChanges) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script");
  std::string scripts_str;
  script_response.SerializeToString(&scripts_str);

  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, scripts_str));
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://b.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, scripts_str));

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

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("autostart"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));

  EXPECT_CALL(mock_client_, AttachUI());
  Start("http://a.example.com/path");
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("runnable");
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  // The script "runnable" stops the flow and shutdowns the controller.
  EXPECT_CALL(mock_client_,
              RecordDropOut(Metrics::DropOutReason::SCRIPT_SHUTDOWN));
  controller_->PerformUserAction(0);
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history state transitions
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, AutostartIsNotPassedToTheUi) {
  SupportsScriptResponseProto script_response;
  auto* autostart = AddRunnableScript(&script_response, "runnable");
  autostart->mutable_presentation()->set_autostart(true);
  RunOnce(autostart);
  SetRepeatedScriptResponse(script_response);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0u)))
      .Times(AnyNumber());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(Gt(0u)))).Times(0);

  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(0));
}

TEST_F(ControllerTest, InitialUrlLoads) {
  GURL initialUrl("http://a.example.com/path");
  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  controller_->Start(initialUrl, TriggerContext::CreateEmpty());
}

TEST_F(ControllerTest, ProgressIncreasesAtStart) {
  EXPECT_EQ(0, controller_->GetProgress());
  EXPECT_CALL(mock_observer_, OnProgressChanged(5));
  Start();
  EXPECT_EQ(5, controller_->GetProgress());
}

TEST_F(ControllerTest, SetProgress) {
  Start();
  EXPECT_CALL(mock_observer_, OnProgressChanged(20));
  controller_->SetProgress(20);
  EXPECT_EQ(20, controller_->GetProgress());
}

TEST_F(ControllerTest, IgnoreProgressDecreases) {
  Start();
  EXPECT_CALL(mock_observer_, OnProgressChanged(Not(15))).Times(AnyNumber());
  controller_->SetProgress(20);
  controller_->SetProgress(15);
  EXPECT_EQ(20, controller_->GetProgress());
}

TEST_F(ControllerTest, SetProgressStep) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(_)).Times(0);
  controller_->SetStepProgressBarConfiguration(config);
  EXPECT_TRUE(controller_->GetStepProgressBarConfiguration().has_value());

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(1)).Times(1);
  controller_->SetProgressActiveStep(1);
  EXPECT_EQ(1, *controller_->GetProgressActiveStep());
}

TEST_F(ControllerTest, IgnoreProgressStepDecreases) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(Not(1)))
      .Times(AnyNumber());
  controller_->SetProgressActiveStep(2);
}

TEST_F(ControllerTest, NewProgressStepConfigurationClampsStep) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3)).Times(1);
  controller_->SetProgressActiveStep(3);
  EXPECT_EQ(3, *controller_->GetProgressActiveStep());

  ShowProgressBarProto::StepProgressBarConfiguration new_config;
  new_config.set_use_step_progress_bar(true);
  new_config.add_annotated_step_icons()->set_identifier("icon1");
  new_config.add_annotated_step_icons()->set_identifier("icon2");
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(2)).Times(1);
  controller_->SetStepProgressBarConfiguration(new_config);
  EXPECT_EQ(2, *controller_->GetProgressActiveStep());
}

TEST_F(ControllerTest, ProgressStepWrapsNegativesToMax) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3)).Times(1);
  controller_->SetProgressActiveStep(-1);
  EXPECT_EQ(3, *controller_->GetProgressActiveStep());
}

TEST_F(ControllerTest, ProgressStepClampsOverflowToMax) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3)).Times(1);
  controller_->SetProgressActiveStep(std::numeric_limits<int>::max());
  EXPECT_EQ(3, *controller_->GetProgressActiveStep());
}

TEST_F(ControllerTest, SetProgressStepFromIdentifier) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(1)).Times(1);
  EXPECT_TRUE(controller_->SetProgressActiveStepIdentifier("icon2"));
  EXPECT_EQ(1, *controller_->GetProgressActiveStep());
}

TEST_F(ControllerTest, SetProgressStepFromUnknownIdentifier) {
  Start();

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.set_use_step_progress_bar(true);
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(_)).Times(0);
  EXPECT_FALSE(controller_->SetProgressActiveStepIdentifier("icon3"));
  EXPECT_FALSE(controller_->GetProgressActiveStep().has_value());
}

TEST_F(ControllerTest, StateChanges) {
  EXPECT_EQ(AutofillAssistantState::INACTIVE, GetUiDelegate()->GetState());

  SupportsScriptResponseProto script_response;
  auto* script1 = AddRunnableScript(&script_response, "script1");
  RunOnce(script1);
  auto* script2 = AddRunnableScript(&script_response, "script2");
  RunOnce(script2);
  SetNextScriptResponse(script_response);

  Start("http://a.example.com/path");
  EXPECT_THAT(states_,
              ElementsAre(AutofillAssistantState::STARTING,
                          AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT));

  // Run script1: State should become RUNNING, as there's another script, then
  // go back to prompt to propose that script.
  states_.clear();
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(2));
  EXPECT_TRUE(controller_->PerformUserAction(0));

  EXPECT_EQ(AutofillAssistantState::PROMPT, GetUiDelegate()->GetState());
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));

  // Run script2: State should become STOPPED, as there are no more runnable
  // scripts.
  states_.clear();
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_TRUE(controller_->PerformUserAction(0));

  EXPECT_EQ(AutofillAssistantState::STOPPED, GetUiDelegate()->GetState());
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED));

  // The cancel button is removed.
  EXPECT_TRUE(controller_->GetUserActions().empty());
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

  Start("http://a.example.com/path");
  // No scripts yet; the element doesn't exit.
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());

  for (int i = 0; i < 3; i++) {
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
  }

  EXPECT_CALL(*mock_web_controller_, OnElementCheck(_, _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT,
            controller_->GetState());
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
              OnGetActions(StrEq("on_timeout_error"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, on_timeout_error_str));

  Start("http://a.example.com/path");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
  EXPECT_EQ("I give up", controller_->GetStatusMessage());
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
              OnGetActions(StrEq("on_timeout_error"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, on_timeout_error_str));

  Start("http://a.example.com/path");

  // Warning after 4s, script succeeds and the client continues to wait.
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
  EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
  EXPECT_EQ("This is slow", controller_->GetStatusMessage());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(AutofillAssistantState::STARTING, controller_->GetState());
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
}

TEST_F(ControllerTest, SuccessfulNavigation) {
  EXPECT_FALSE(controller_->IsNavigatingToNewDocument());
  EXPECT_FALSE(controller_->HasNavigationError());

  NavigationStateChangeListener listener(controller_.get());
  controller_->AddNavigationListener(&listener);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetMainFrame());
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
      web_contents()->GetMainFrame());
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
          GURL("http://original.example.com/"), web_contents()->GetMainFrame());
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
      web_contents()->GetMainFrame());
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetMainFrame());
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
      GURL("http://initialurl.com"), web_contents()->GetMainFrame());
  listener.events.clear();
  controller_->RemoveNavigationListener(&listener);

  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL("http://initialurl.com"), net::ERR_CONNECTION_TIMED_OUT,
      web_contents()->GetMainFrame());
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://initialurl.com"), web_contents()->GetMainFrame());

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
  EXPECT_CALL(*mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Start script, which waits for some navigation event to happen after the
  // expect_navigation action has run..
  EXPECT_TRUE(controller_->PerformUserAction(0));

  // No navigation event happened within the action timeout and the script ends.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

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
  EXPECT_CALL(*mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  Start("http://a.example.com/path");
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Start script, which waits for some navigation event to happen after the
  // expect_navigation action has run..
  EXPECT_TRUE(controller_->PerformUserAction(0));

  // Navigation starts, but does not end, within the timeout.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://a.example.com/path"), web_contents()->GetMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Navigation finishes and the script ends.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  simulator->Commit();

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
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
              OnGetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(GURL("http://b.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Execute the script, which requires showing the UI, then go back to tracking
  // mode
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(controller_->PerformUserAction(0));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Move to a domain for which there are no scripts. This causes the controller
  // to stop.
  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::STOPPED));

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

  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  EXPECT_TRUE(controller_->PerformUserAction(0));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackScriptShowUIOnTell) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "runnable");
  script->mutable_presentation()->set_needs_ui(false);
  SetupScripts(script_response);

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("error");
  SetupActionsForScript("runnable", runnable_script);

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(controller_->PerformUserAction(0));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // As the controller is back in tracking mode; A UI is not needed anymore.
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
  EXPECT_CALL(*mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  EXPECT_FALSE(controller_->NeedsUI());
  EXPECT_CALL(mock_client_, AttachUI());
  EXPECT_TRUE(controller_->PerformUserAction(0));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // As the controller is back in tracking mode; A UI is not needed anymore.
  EXPECT_FALSE(controller_->NeedsUI());

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
              OnGetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

  // Start tracking at example.com, with one script matching
  SetLastCommittedUrl(GURL("http://example.com/"));

  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("runnable"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));

  // When the script fails, the controller transitions to STOPPED state, then
  // right away back to TRACKING state.
  EXPECT_TRUE(controller_->PerformUserAction(0));
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Check the full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::STOPPED,
                                   AutofillAssistantState::TRACKING));
}

TEST_F(ControllerTest, TrackReportsFirstSetOfScripts) {
  Service::ResponseCallback get_scripts_callback;
  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
      .WillOnce(
          Invoke([&get_scripts_callback](const GURL& url,
                                         const TriggerContext& trigger_context,
                                         Service::ResponseCallback& callback) {
            get_scripts_callback = std::move(callback);
          }));

  SetLastCommittedUrl(GURL("http://example.com/"));
  bool first_check_done = false;
  controller_->Track(TriggerContext::CreateEmpty(),
                     base::BindOnce(
                         [](Controller* controller, bool* is_done) {
                           // User actions must have been set when this is
                           // called
                           EXPECT_THAT(controller->GetUserActions(), SizeIs(1));
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
  std::move(get_scripts_callback).Run(true, response_str);

  EXPECT_TRUE(first_check_done);
  EXPECT_TRUE(controller_->HasRunFirstCheck());
}

TEST_F(ControllerTest, TrackReportsNoScripts) {
  SetLastCommittedUrl(GURL("http://example.com/"));
  base::MockCallback<base::OnceCallback<void()>> callback;

  EXPECT_CALL(callback, Run());
  controller_->Track(TriggerContext::CreateEmpty(), callback.Get());
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
  controller_->Track(TriggerContext::CreateEmpty(), callback.Get());
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
  controller_->Track(TriggerContext::CreateEmpty(), callback.Get());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
}

TEST_F(ControllerTest, TrackReportsAlreadyDone) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  base::MockCallback<base::OnceCallback<void()>> callback;
  EXPECT_CALL(callback, Run());
  controller_->Track(TriggerContext::CreateEmpty(), callback.Get());
}

TEST_F(ControllerTest, TrackThenAutostart) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("autostart"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));

  ActionsResponseProto runnable_script;
  runnable_script.add_actions()->mutable_tell()->set_message("runnable");
  runnable_script.add_actions()->mutable_stop();
  SetupActionsForScript("runnable", runnable_script);

  EXPECT_CALL(mock_client_, AttachUI());
  Start("http://example.com/");
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(), SizeIs(1));

  // Run "runnable", which then calls stop and ends. The controller should then
  // go back to TRACKING mode.
  controller_->PerformUserAction(0);
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::TRACKING,
                                   AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::TRACKING));
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
              OnGetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(GURL("http://b.example.com/"), _, _))
      .Times(0);
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(GURL("http://c.example.com/"), _, _))
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

TEST_F(ControllerTest, BrowseStateWithDomainWhitelist) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  *prompt->add_browse_domains_whitelist() = "example.com";
  *prompt->add_browse_domains_whitelist() = "other-example.com";
  prompt->add_choices()->mutable_chip()->set_text("continue");
  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

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

  // Same domain navigations as one of the whitelisted domains should not
  // shutdown AA.
  SimulateNavigateToUrl(GURL("http://other-example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());
}

TEST_F(ControllerTest, BrowseStateWithDomainWhitelistCleanup) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  ActionsResponseProto runnable_script;
  auto* prompt = runnable_script.add_actions()->mutable_prompt();
  prompt->set_browse_mode(true);
  *prompt->add_browse_domains_whitelist() = "example.com";
  prompt->add_choices()->mutable_chip()->set_text("continue");

  // Second browse action without a whitelist.
  auto* prompt2 = runnable_script.add_actions()->mutable_prompt();
  prompt2->set_browse_mode(true);
  prompt2->add_choices()->mutable_chip()->set_text("done");

  SetupActionsForScript("runnable", runnable_script);
  std::string response_str;
  script_response.SerializeToString(&response_str);
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  SimulateNavigateToUrl(GURL("http://b.example.com/"));
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  // Click "continue".
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "continue");
  controller_->PerformUserAction(0);

  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "done");

  // Make sure the whitelist got reset with the second prompt action.
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
              OnGetScriptsForUrl(GURL("http://example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

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
  std::string never_shown = "never shown";
  runnable_script.add_actions()->mutable_tell()->set_message(never_shown);
  SetupActionsForScript("runnable", runnable_script);

  SetLastCommittedUrl(GURL("http://example.com/"));
  controller_->Track(TriggerContext::CreateEmpty(), base::DoNothing());
  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "runnable");

  // Start the script, which should show a prompt with the continue chip.
  controller_->PerformUserAction(0);
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "continue");

  // Browser (not document) initiated navigation while in prompt mode (such as
  // go back): The controller stops the scripts, shows an error, then goes back
  // to tracking mode.
  //
  // The tell never_shown which follows the prompt action should never be
  // executed.
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(never_shown)).Times(0);
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(testing::Not(never_shown)))
      .Times(testing::AnyNumber());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/otherpage"));

  EXPECT_EQ(AutofillAssistantState::TRACKING, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "runnable");

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
  std::string never_shown = "never shown";
  autostart_script.add_actions()->mutable_tell()->set_message(never_shown);
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "continue");

  // Browser (not document) initiated navigation while in prompt mode (such as
  // go back): The controller stops the scripts, shows an error and shuts down.
  //
  // The tell never_shown which follows the prompt action should never be
  // executed.
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(never_shown)).Times(0);
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(testing::Not(never_shown)))
      .Times(testing::AnyNumber());

  // Renderer (Document) initiated navigation is allowed.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://a.example.com/page"), web_contents()->GetMainFrame());
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

TEST_F(ControllerTest, UnexpectedNavigationDuringLiteScriptPromptAction) {
  ON_CALL(*mock_service_, IsLiteService).WillByDefault(Return(true));

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
  autostart_script.add_actions()->mutable_tell()->set_message("never shown");
  SetupActionsForScript("autostart", autostart_script);

  Start();
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "continue");

  // No error is shown for lite scripts.
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(_)).Times(0);

  // Renderer (Document) initiated navigation is allowed.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://a.example.com/page"), web_contents()->GetMainFrame());
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Expected browser initiated navigation is allowed.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  controller_->ExpectNavigation();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://b.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Unexpected browser initiated navigation is allowed for lite scripts.
  EXPECT_CALL(mock_client_, Shutdown(_)).Times(0);
  EXPECT_CALL(mock_client_, RecordDropOut(_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://c.example.com/page"));
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));
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
      GURL("http://a.example.com/page"), web_contents()->GetMainFrame());
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
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(_));
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

TEST_F(ControllerTest, NavigationToGooglePropertyDestroysUI) {
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

  EXPECT_CALL(mock_client_, RecordDropOut(Metrics::DropOutReason::NAVIGATION));
  EXPECT_CALL(mock_client_, DestroyUI);
  GURL google("https://google.com/search");
  SetLastCommittedUrl(google);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             google);

  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, DomainChangeToGooglePropertyDuringBrowseDestroysUI) {
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
              OnGetScriptsForUrl(GURL("http://a.example.com/"), _, _))
      .WillOnce(RunOnceCallback<2>(true, response_str));

  Start("http://a.example.com/");
  EXPECT_EQ(AutofillAssistantState::BROWSE, controller_->GetState());

  EXPECT_CALL(
      mock_client_,
      RecordDropOut(Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE));
  EXPECT_CALL(mock_client_, DestroyUI);
  GURL google("https://google.com/search");
  SetLastCommittedUrl(google);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             google);

  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  // Full history of state transitions.
  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::BROWSE,
                                   AutofillAssistantState::STOPPED));
}

TEST_F(ControllerTest, UserDataFormEmpty) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  // Request nothing, expect continue button to be enabled.
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnCollectUserDataOptionsChanged(Not(nullptr)))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr), UserData::FieldChange::ALL))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());
}

TEST_F(ControllerTest, UserDataFormContactInfo) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->request_payer_name = true;
  options->request_payer_email = true;
  options->request_payer_phone = true;
  options->contact_details_name = "selected_profile";

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(
      mock_observer_,
      OnUserDataChanged(Not(nullptr), UserData::FieldChange::CONTACT_PROFILE))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);

  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             base::UTF8ToUTF16("joedoe@example.com"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                             base::UTF8ToUTF16("Joe Doe"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("+1 23 456 789 01"));
  controller_->SetContactInfo(
      std::make_unique<autofill::AutofillProfile>(contact_profile));
  EXPECT_THAT(controller_->GetUserData()
                  ->selected_address("selected_profile")
                  ->Compare(contact_profile),
              Eq(0));
}

TEST_F(ControllerTest, UserDataFormCreditCard) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->request_payment_method = true;
  options->billing_address_name = "billing_address";
  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  // Credit card without billing address is invalid.
  auto credit_card = std::make_unique<autofill::CreditCard>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetCreditCardInfo(credit_card.get(), "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    /* billing_address_id = */ "");
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr), UserData::FieldChange::CARD))
      .Times(1);
  EXPECT_CALL(
      mock_observer_,
      OnUserDataChanged(Not(nullptr), UserData::FieldChange::BILLING_ADDRESS))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCreditCard(
      std::make_unique<autofill::CreditCard>(*credit_card),
      /* billing_profile =*/nullptr);

  // Credit card with valid billing address is ok.
  auto billing_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetProfileInfo(billing_address.get(), "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");
  credit_card->set_billing_address_id(billing_address->guid());
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr), UserData::FieldChange::CARD))
      .Times(1);
  EXPECT_CALL(
      mock_observer_,
      OnUserDataChanged(Not(nullptr), UserData::FieldChange::BILLING_ADDRESS))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  controller_->SetCreditCard(
      std::make_unique<autofill::CreditCard>(*credit_card),
      std::make_unique<autofill::AutofillProfile>(*billing_address));
  EXPECT_THAT(GetUserData()->selected_card_->Compare(*credit_card), Eq(0));
  EXPECT_THAT(GetUserData()->selected_addresses_["billing_address"]->Compare(
                  *billing_address),
              Eq(0));
}

TEST_F(ControllerTest, UserDataChangesByOutOfLoopWrite) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->request_payer_name = true;
  options->request_payer_email = true;
  options->request_payer_phone = true;
  options->contact_details_name = "selected_profile";

  testing::InSequence sequence;

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             base::UTF8ToUTF16("joedoe@example.com"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                             base::UTF8ToUTF16("Joe Doe"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("+1 23 456 789 01"));
  controller_->SetContactInfo(
      std::make_unique<autofill::AutofillProfile>(contact_profile));
  EXPECT_THAT(controller_->GetUserData()
                  ->selected_address("selected_profile")
                  ->Compare(contact_profile),
              Eq(0));

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  // Can be called by a PDM update.
  controller_->WriteUserData(base::BindOnce(
      [](UserData* user_data, UserData::FieldChange* field_change) {
        auto it = user_data->selected_addresses_.find("selected_profile");
        if (it != user_data->selected_addresses_.end()) {
          user_data->selected_addresses_.erase(it);
          *field_change = UserData::FieldChange::CONTACT_PROFILE;
        }
      }));
}

TEST_F(ControllerTest, SetTermsAndConditions) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->accept_terms_and_conditions_text.assign("Accept T&C");
  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::TERMS_AND_CONDITIONS))
      .Times(1);
  controller_->SetTermsAndConditions(TermsAndConditionsState::ACCEPTED);
  EXPECT_THAT(controller_->GetUserData()->terms_and_conditions_,
              Eq(TermsAndConditionsState::ACCEPTED));
}

TEST_F(ControllerTest, SetLoginOption) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->request_login_choice = true;
  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  EXPECT_CALL(
      mock_observer_,
      OnUserDataChanged(Not(nullptr), UserData::FieldChange::LOGIN_CHOICE))
      .Times(1);
  controller_->SetLoginOption("1");
  EXPECT_THAT(controller_->GetUserData()->login_choice_identifier_, Eq("1"));
}

TEST_F(ControllerTest, SetShippingAddress) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->request_shipping = true;
  options->shipping_address_name = "shipping_address";
  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  auto shipping_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetProfileInfo(shipping_address.get(), "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  EXPECT_CALL(
      mock_observer_,
      OnUserDataChanged(Not(nullptr), UserData::FieldChange::SHIPPING_ADDRESS))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  controller_->SetShippingAddress(
      std::make_unique<autofill::AutofillProfile>(*shipping_address));
  EXPECT_THAT(GetUserData()->selected_addresses_["shipping_address"]->Compare(
                  *shipping_address),
              Eq(0));
}

TEST_F(ControllerTest, SetAdditionalValues) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  ValueProto value1;
  value1.mutable_strings()->add_values("123456789");

  base::OnceCallback<void(UserData*, UserData::FieldChange*)> callback =
      base::BindLambdaForTesting(
          [&](UserData* user_data, UserData::FieldChange* change) {
            ValueProto value2;
            value2.mutable_strings()->add_values("");
            ValueProto value3;
            value3.mutable_strings()->add_values("");
            user_data->additional_values_["key1"] = value1;
            user_data->additional_values_["key2"] = value2;
            user_data->additional_values_["key3"] = value3;
            *change = UserData::FieldChange::ADDITIONAL_VALUES;
          });

  controller_->WriteUserData(std::move(callback));

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))))
      .Times(1);
  controller_->SetCollectUserDataOptions(options.get());

  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                    Property(&UserAction::enabled, Eq(true)))))
        .Times(1);
    EXPECT_CALL(mock_observer_,
                OnUserDataChanged(Not(nullptr),
                                  UserData::FieldChange::ADDITIONAL_VALUES))
        .Times(1);
  }
  ValueProto value4;
  value4.mutable_strings()->add_values("value2");
  ValueProto value5;
  value5.mutable_strings()->add_values("value3");
  controller_->SetAdditionalValue("key2", value4);
  controller_->SetAdditionalValue("key3", value5);
  EXPECT_EQ(controller_->GetUserData()->additional_values_.at("key1"), value1);
  EXPECT_EQ(controller_->GetUserData()->additional_values_.at("key2"), value4);
  EXPECT_EQ(controller_->GetUserData()->additional_values_.at("key3"), value5);

  ValueProto value6;
  value6.mutable_strings()->add_values("someValue");
  EXPECT_DCHECK_DEATH(controller_->SetAdditionalValue("key4", value6));
}

TEST_F(ControllerTest, SetOverlayColors) {
  EXPECT_CALL(
      mock_observer_,
      OnOverlayColorsChanged(AllOf(
          Field(&Controller::OverlayColors::background, StrEq("#FF000000")),
          Field(&Controller::OverlayColors::highlight_border,
                StrEq("#FFFFFFFF")))));

  std::map<std::string, std::string> parameters;
  parameters["OVERLAY_COLORS"] = "#FF000000:#FFFFFFFF";
  auto context = TriggerContext::Create(parameters, "exps");

  GURL url("http://a.example.com/path");
  controller_->Start(url, std::move(context));
}

TEST_F(ControllerTest, SetDateTimeRange) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);
  DateProto start_date;
  start_date.set_year(2020);
  start_date.set_month(1);
  start_date.set_day(20);
  controller_->SetDateTimeRangeStartDate(start_date);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->month(),
            1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->day(), 20);

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);
  controller_->SetDateTimeRangeStartTimeSlot(0);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_timeslot_, 0);

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);
  DateProto end_date;
  end_date.set_year(2020);
  end_date.set_month(1);
  end_date.set_day(25);
  controller_->SetDateTimeRangeEndDate(end_date);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->month(), 1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->day(), 25);

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);
  controller_->SetDateTimeRangeEndTimeSlot(1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_timeslot_, 1);
}

TEST_F(ControllerTest, SetDateTimeRangeStartDateAfterEndDate) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto date;
  date.set_year(2020);
  date.set_month(1);
  date.set_day(20);
  GetUserData()->date_time_range_start_date_ = date;
  GetUserData()->date_time_range_end_date_ = date;

  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);

  date.set_day(21);
  controller_->SetDateTimeRangeStartDate(date);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->month(),
            1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->day(), 21);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_,
            base::nullopt);
}

TEST_F(ControllerTest, SetDateTimeRangeEndDateBeforeStartDate) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto date;
  date.set_year(2020);
  date.set_month(1);
  date.set_day(20);
  GetUserData()->date_time_range_start_date_ = date;
  GetUserData()->date_time_range_end_date_ = date;

  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);

  date.set_day(19);
  controller_->SetDateTimeRangeEndDate(date);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->month(), 1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->day(), 19);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_,
            base::nullopt);
}

TEST_F(ControllerTest, SetDateTimeRangeSameDatesStartTimeAfterEndTime) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto date;
  date.set_year(2020);
  date.set_month(1);
  date.set_day(20);
  GetUserData()->date_time_range_start_date_ = date;
  GetUserData()->date_time_range_end_date_ = date;
  GetUserData()->date_time_range_end_timeslot_ = 0;

  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);

  controller_->SetDateTimeRangeStartTimeSlot(1);
  EXPECT_EQ(*controller_->GetUserData()->date_time_range_start_timeslot_, 1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_timeslot_,
            base::nullopt);
}

TEST_F(ControllerTest, SetDateTimeRangeSameDatesEndTimeBeforeStartTime) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto date;
  date.set_year(2020);
  date.set_month(1);
  date.set_day(20);
  GetUserData()->date_time_range_start_date_ = date;
  GetUserData()->date_time_range_end_date_ = date;
  GetUserData()->date_time_range_start_timeslot_ = 1;

  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);

  controller_->SetDateTimeRangeEndTimeSlot(0);
  EXPECT_EQ(*controller_->GetUserData()->date_time_range_end_timeslot_, 0);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_timeslot_,
            base::nullopt);
}

TEST_F(ControllerTest, SetDateTimeRangeSameDateValidTime) {
  testing::InSequence seq;

  auto options = std::make_unique<MockCollectUserDataOptions>();
  options->request_date_time_range = true;
  auto* time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options->date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto date;
  date.set_year(2020);
  date.set_month(1);
  date.set_day(20);
  GetUserData()->date_time_range_start_date_ = date;
  GetUserData()->date_time_range_end_date_ = date;

  controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_START))
      .Times(1);
  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::DATE_TIME_RANGE_END))
      .Times(1);
  controller_->SetDateTimeRangeStartTimeSlot(0);
  controller_->SetDateTimeRangeEndTimeSlot(1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->month(),
            1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_date_->day(), 20);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->year(),
            2020);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->month(), 1);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_end_date_->day(), 20);
  EXPECT_EQ(controller_->GetUserData()->date_time_range_start_timeslot_, 0);
  EXPECT_EQ(*controller_->GetUserData()->date_time_range_end_timeslot_, 1);
}

TEST_F(ControllerTest, ChangeClientSettings) {
  SupportsScriptResponseProto response;
  response.mutable_client_settings()->set_periodic_script_check_interval_ms(1);
  SetupScripts(response);
  EXPECT_CALL(mock_observer_,
              OnClientSettingsChanged(
                  Field(&ClientSettings::periodic_script_check_interval,
                        base::TimeDelta::FromMilliseconds(1))));
  Start();
}

TEST_F(ControllerTest, WriteUserData) {
  auto options = std::make_unique<MockCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();
  controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(mock_observer_,
              OnUserDataChanged(Not(nullptr),
                                UserData::FieldChange::TERMS_AND_CONDITIONS))
      .Times(1);

  base::OnceCallback<void(UserData*, UserData::FieldChange*)> callback =
      base::BindOnce([](UserData* data, UserData::FieldChange* change) {
        data->terms_and_conditions_ = TermsAndConditionsState::ACCEPTED;
        *change = UserData::FieldChange::TERMS_AND_CONDITIONS;
      });

  controller_->WriteUserData(std::move(callback));
  EXPECT_EQ(GetUserData()->terms_and_conditions_,
            TermsAndConditionsState::ACCEPTED);
}

TEST_F(ControllerTest, ExpandOrCollapseBottomSheet) {
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer_, OnCollapseBottomSheet()).Times(1);
    EXPECT_CALL(mock_observer_, OnExpandBottomSheet()).Times(1);
  }
  controller_->CollapseBottomSheet();
  controller_->ExpandBottomSheet();
}

TEST_F(ControllerTest, ShouldPromptActionExpandSheet) {
  // Expect this to be true initially.
  EXPECT_TRUE(controller_->ShouldPromptActionExpandSheet());

  controller_->SetExpandSheetForPromptAction(false);
  EXPECT_FALSE(controller_->ShouldPromptActionExpandSheet());

  controller_->SetExpandSheetForPromptAction(true);
  EXPECT_TRUE(controller_->ShouldPromptActionExpandSheet());
}

TEST_F(ControllerTest, SecondPromptActionShouldDefaultToExpandSheet) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  ActionsResponseProto runnable_script;
  // Prompt action 1 which disables auto expand.
  auto* prompt_action = runnable_script.add_actions()->mutable_prompt();
  prompt_action->add_choices()->mutable_chip()->set_text("continue");
  prompt_action->set_disable_force_expand_sheet(true);

  // Prompt action 2 using the default should fall back to auto expand again.
  runnable_script.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("next");

  SetupActionsForScript("runnable", runnable_script);
  Start();

  // The first prompt should not auto expand.
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_FALSE(controller_->ShouldPromptActionExpandSheet());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "continue");

  // Click "continue"
  EXPECT_TRUE(controller_->PerformUserAction(0));

  // The second prompt should fall back to default auto expand again.
  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_TRUE(controller_->ShouldPromptActionExpandSheet());
  ASSERT_THAT(controller_->GetUserActions(), SizeIs(1));
  EXPECT_EQ(controller_->GetUserActions()[0].chip().text, "next");
}

TEST_F(ControllerTest, SetGenericUi) {
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer_, OnGenericUserInterfaceChanged(NotNull()));
    EXPECT_CALL(mock_observer_, OnGenericUserInterfaceChanged(nullptr));
  }
  controller_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(GenericUserInterfaceProto()),
      base::DoNothing(), base::DoNothing());
  controller_->ClearGenericUi();
}

TEST_F(ControllerTest, StartPasswordChangeFlow) {
  GURL initialUrl("http://example.com/password");
  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  std::map<std::string, std::string> parameters;
  std::string username = "test_username";
  parameters["PASSWORD_CHANGE_USERNAME"] = username;

  EXPECT_TRUE(
      controller_->Start(initialUrl, TriggerContext::Create(parameters, "")));
  EXPECT_EQ(GetUserData()->selected_login_->username, username);
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
  EXPECT_CALL(*mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  Start("http://a.example.com/path");

  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("ok")))));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("http://a.example.com/path"), web_contents()->GetMainFrame());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Commit the navigation, which will end the current prompt.
  EXPECT_THAT(processed_actions_capture, SizeIs(0));
  simulator->Commit();

  EXPECT_EQ(AutofillAssistantState::PROMPT, controller_->GetState());
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   Field(&Chip::text, StrEq("ok 2")))));

  EXPECT_TRUE(controller_->PerformUserAction(0));

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

TEST_F(ControllerTest, PauseAndResume) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello World");
  auto* action = actions_response.add_actions()->mutable_prompt();
  action->add_choices()->mutable_chip()->set_text("ok");

  SetupActionsForScript("script", actions_response);
  Start("http://a.example.com/path");

  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));
  EXPECT_THAT(controller_->GetStatusMessage(), StrEq("Hello World"));
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   AllOf(Field(&Chip::text, StrEq("ok")),
                                         Field(&Chip::type, NORMAL_ACTION)))));

  ScriptExecutorListener listener;
  controller_->AddListener(&listener);
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Stop"));
  controller_->OnStop("Stop", "Undo");
  EXPECT_EQ(1, listener.pause_count);
  controller_->RemoveListener(&listener);

  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());
  EXPECT_THAT(controller_->GetStatusMessage(), StrEq("Stop"));
  EXPECT_THAT(
      controller_->GetUserActions(),
      ElementsAre(Property(&UserAction::chip,
                           AllOf(Field(&Chip::text, StrEq("Undo")),
                                 Field(&Chip::type, HIGHLIGHTED_ACTION)))));

  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Hello World"));
  EXPECT_TRUE(controller_->PerformUserAction(0));

  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT,
                                   AutofillAssistantState::STOPPED,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));
  EXPECT_THAT(controller_->GetStatusMessage(), StrEq("Hello World"));
  EXPECT_THAT(controller_->GetUserActions(),
              ElementsAre(Property(&UserAction::chip,
                                   AllOf(Field(&Chip::text, StrEq("ok")),
                                         Field(&Chip::type, NORMAL_ACTION)))));
}

TEST_F(ControllerTest, PauseAndNavigate) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello World");
  auto* action = actions_response.add_actions()->mutable_prompt();
  action->add_choices()->mutable_chip()->set_text("ok");

  SetupActionsForScript("script", actions_response);
  Start("http://a.example.com/path");

  EXPECT_THAT(states_, ElementsAre(AutofillAssistantState::STARTING,
                                   AutofillAssistantState::RUNNING,
                                   AutofillAssistantState::PROMPT));
  controller_->OnStop("Stop", "Undo");

  EXPECT_EQ(AutofillAssistantState::STOPPED, controller_->GetState());

  EXPECT_CALL(mock_client_, Shutdown(Metrics::DropOutReason::NAVIGATION));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://b.example.com/path"));
}

TEST_F(ControllerTest,
       LiteScriptWithOnboardingDoesNotShowInitialStatusMessage) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello World");

  SetupActionsForScript("script", actions_response);
  auto trigger_context = std::make_unique<TriggerContextImpl>(
      std::map<std::string, std::string>{
          {"TRIGGER_SCRIPT_USED", "example/path"}},
      /* exp = */ std::string());
  trigger_context->SetOnboardingShown(true);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_,
              OnStatusMessageChanged(testing::Not("Hello World")))
      .Times(0);
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Hello World")).Times(1);
  Start("http://a.example.com/path", std::move(trigger_context));
}

TEST_F(ControllerTest, RegularScriptShowsDefaultInitialStatusMessage) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script")
      ->mutable_presentation()
      ->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello World");

  SetupActionsForScript("script", actions_response);

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(l10n_util::GetStringFUTF8(
                                  IDS_AUTOFILL_ASSISTANT_LOADING,
                                  base::UTF8ToUTF16("a.example.com"))))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("Hello World")).Times(1);
  Start("http://a.example.com/path");
}

TEST_F(ControllerTest, NotifyRuntimeManagerOnUiStateChange) {
  EXPECT_CALL(mock_runtime_manager_, SetUIState(UIState::kShown)).Times(1);
  controller_->SetUiShown(true);

  EXPECT_CALL(mock_runtime_manager_, SetUIState(UIState::kNotShown)).Times(1);
  controller_->SetUiShown(false);
}
}  // namespace autofill_assistant
