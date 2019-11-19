// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <map>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;

const char* kScriptPath = "script_path";

class ScriptExecutorTest : public testing::Test,
                           public ScriptExecutor::Listener {
 public:
  void SetUp() override {
    delegate_.SetService(&mock_service_);
    delegate_.SetWebController(&mock_web_controller_);
    delegate_.SetCurrentURL(GURL("http://example.com/"));

    std::map<std::string, std::string> script_parameters;
    script_parameters["additional_param"] = "additional_param_value";
    executor_ = std::make_unique<ScriptExecutor>(
        kScriptPath,
        TriggerContext::Create(script_parameters, "additional_exp"),
        /* global_payload= */ "initial global payload",
        /* script_payload= */ "initial payload",
        /* listener= */ this, &scripts_state_, &ordered_interrupts_,
        /* delegate= */ &delegate_);

    // In this test, "tell" actions always succeed and "click" actions always
    // fail. The following makes a click action fail immediately
    ON_CALL(mock_web_controller_, OnClickOrTapElement(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus(OTHER_ACTION_STATUS)));

    ON_CALL(mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_, OnFocusElement(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
  }

 protected:
  ScriptExecutorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Implements ScriptExecutor::Listener
  void OnServerPayloadChanged(const std::string& global_payload,
                              const std::string& script_payload) override {
    last_global_payload_ = global_payload;
    last_script_payload_ = script_payload;
  }

  void OnScriptListChanged(
      std::vector<std::unique_ptr<Script>> scripts) override {
    should_update_scripts_ = true;
    scripts_update_ = std::move(scripts);
    ++scripts_update_count_;
  }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  // Creates a script that contains a wait_for_dom allow_interrupt=true followed
  // by a tell. It will succeed if |element| eventually becomes visible.
  void SetupInterruptibleScript(const std::string& path,
                                const std::string& element) {
    ActionsResponseProto interruptible;
    interruptible.set_global_payload("main script global payload");
    interruptible.set_script_payload("main script payload");
    auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
    wait_action->mutable_wait_until()->add_selectors(element);
    wait_action->set_allow_interrupt(true);
    interruptible.add_actions()->mutable_tell()->set_message(path);
    EXPECT_CALL(mock_service_, OnGetActions(StrEq(path), _, _, _, _, _))
        .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interruptible)));
  }

  // Creates an interrupt that contains a tell. It will always succeed.
  void SetupInterrupt(const std::string& path, const std::string& trigger) {
    RegisterInterrupt(path, trigger);

    ActionsResponseProto interrupt_actions;
    InitInterruptActions(&interrupt_actions, path);
    EXPECT_CALL(mock_service_, OnGetActions(StrEq(path), _, _, _, _, _))
        .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));
  }

  void InitInterruptActions(ActionsResponseProto* interrupt_actions,
                            const std::string& path) {
    interrupt_actions->set_global_payload(
        base::StrCat({"global payload for ", path}));
    interrupt_actions->set_script_payload(base::StrCat({"payload for ", path}));
    interrupt_actions->add_actions()->mutable_tell()->set_message(path);
  }

  // Registers an interrupt, but do not define actions for it.
  void RegisterInterrupt(const std::string& path, const std::string& trigger) {
    auto interrupt = std::make_unique<Script>();
    interrupt->handle.path = path;
    ScriptPreconditionProto interrupt_preconditions;
    interrupt_preconditions.add_elements_exist()->add_selectors(trigger);
    interrupt->precondition =
        ScriptPrecondition::FromProto(path, interrupt_preconditions);

    ordered_interrupts_.push_back(interrupt.get());
    interrupts_.emplace_back(std::move(interrupt));
  }

  // task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_environment_;
  FakeScriptExecutorDelegate delegate_;
  Script script_;
  StrictMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  std::map<std::string, ScriptStatusProto> scripts_state_;

  // An owner for the pointers in |ordered_interrupts_|
  std::vector<std::unique_ptr<Script>> interrupts_;
  std::vector<Script*> ordered_interrupts_;
  std::string last_global_payload_;
  std::string last_script_payload_;
  bool should_update_scripts_ = false;
  std::vector<std::unique_ptr<Script>> scripts_update_;
  int scripts_update_count_ = 0;
  std::unique_ptr<ScriptExecutor> executor_;
  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;
};

TEST_F(ScriptExecutorTest, GetActionsFails) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, false),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardParameters) {
  std::map<std::string, std::string> parameters;
  parameters["param"] = "value";
  delegate_.SetTriggerContext(TriggerContext::Create(parameters, "exp"));
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(Invoke([](const std::string& script_path, const GURL& url,
                          const TriggerContext& trigger_context,
                          const std::string& global_payload,
                          const std::string& script_payload,
                          Service::ResponseCallback& callback) {
        // |trigger_context| includes data passed to
        // ScriptExecutor constructor as well as data from the
        // delegate's TriggerContext.
        EXPECT_THAT("exp,additional_exp", trigger_context.experiment_ids());
        EXPECT_THAT(
            "additional_param_value",
            trigger_context.GetParameter("additional_param").value_or(""));
        EXPECT_THAT("value",
                    trigger_context.GetParameter("param").value_or(""));

        std::move(callback).Run(true, "");
      }));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunOneActionReportAndReturn) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].has_run_time_ms());
  EXPECT_GE(processed_actions_capture[0].run_time_ms(), 0);
}

TEST_F(ScriptExecutorTest, RunMultipleActions) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("3");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions1_capture),
                RunOnceCallback<4>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions2_capture),
                      RunOnceCallback<4>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(1u, processed_actions2_capture.size());
}

TEST_F(ScriptExecutorTest, UnsupportedAction) {
  ActionsResponseProto actions_response;
  actions_response.add_actions();  // action definition missing

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(UNSUPPORTED_ACTION, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StopAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ResetAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_reset();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::RESTART))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, InterruptActionListOnError) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "will pass");
  initial_actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "never run");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message(
      "will run after error");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions1_capture),
                RunOnceCallback<4>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions2_capture),
                      RunOnceCallback<4>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions1_capture[1].status());

  ASSERT_EQ(1u, processed_actions2_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
  // make sure "never run" wasn't the one that was run.
  EXPECT_EQ("will run after error",
            processed_actions2_capture[0].action().tell().message());
}

TEST_F(ScriptExecutorTest, RunDelayedAction) {
  ActionsResponseProto actions_response;
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("delayed");
  action->set_action_delay_ms(1000);

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // executor_callback_.Run() not expected to be run just yet, as the action is
  // delayed.
  executor_->Run(executor_callback_.Get());
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Moving forward in time triggers action execution.
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(ScriptExecutorTest, ClearDetailsWhenFinished) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");

  *actions_response.add_actions() = click_with_clean_contextual_ui;

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  delegate_.SetDetails(std::make_unique<Details>());  // empty, but not null
  executor_->Run(executor_callback_.Get());
  EXPECT_EQ(nullptr, delegate_.GetDetails());
}

TEST_F(ScriptExecutorTest, DontClearDetailsIfOtherActionsAreLeft) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");
  *actions_response.add_actions() = click_with_clean_contextual_ui;
  actions_response.add_actions()->mutable_tell()->set_message("Wait no!");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  delegate_.SetDetails(std::make_unique<Details>());  // empty, but not null
  executor_->Run(executor_callback_.Get());
  EXPECT_NE(nullptr, delegate_.GetDetails());
}

TEST_F(ScriptExecutorTest, ClearDetailsOnError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello");
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  delegate_.SetDetails(std::make_unique<Details>());  // empty, but not null
  executor_->Run(executor_callback_.Get());
  EXPECT_EQ(nullptr, delegate_.GetDetails());
}

TEST_F(ScriptExecutorTest, UpdateScriptStateWhileRunning) {
  // OnGetNextActions never calls the callback, so Run() returns immediately
  // without doing anything.
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _));

  EXPECT_THAT(scripts_state_, IsEmpty());
  executor_->Run(executor_callback_.Get());
  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_RUNNING)));
}

TEST_F(ScriptExecutorTest, UpdateScriptStateOnError) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_FAILURE)));
}

TEST_F(ScriptExecutorTest, UpdateScriptStateOnSuccess) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("ok");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnSuccess) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, "initial global payload",
                                          "initial payload", _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_global_payload("last global payload");
  next_actions_response.set_script_payload("last payload");
  EXPECT_CALL(mock_service_, OnGetNextActions(_, "actions global payload",
                                              "actions payload", _, _))
      .WillOnce(RunOnceCallback<4>(true, Serialize(next_actions_response)));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("last global payload", last_global_payload_);
  EXPECT_EQ("last payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnError) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, "initial global payload",
                                          "initial payload", _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, "actions global payload",
                                              "actions payload", _, _))
      .WillOnce(RunOnceCallback<4>(false, ""));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("actions global payload", last_global_payload_);
  EXPECT_EQ("actions payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, WaitForDomWaitUntil) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // First check does not find the element, wait for dom waits 1s, then the
  // element is found, and the action succeeds.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  executor_->Run(executor_callback_.Get());

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(executor_callback_, Run(_));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, WaitForDomWaitWhile) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->mutable_wait_while()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // First check finds the element, wait for dom waits 1s, then the element
  // disappears, and the action succeeds.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  executor_->Run(executor_callback_.Get());

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(executor_callback_, Run(_));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunInterrupt) {
  // All elements exist, so first the interrupt should be run, then the element
  // should be reported as found.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  // Both scripts ends after the first set of actions. Capture the results.
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions1_capture),
                      RunOnceCallback<4>(true, "")))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions2_capture),
                      RunOnceCallback<4>(true, "")));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));

  // The first script to call OnGetNextActions is the interrupt, which starts
  // with a tell.
  ASSERT_THAT(processed_actions1_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kTell,
            processed_actions1_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());

  // The second script to call OnGetNextActions is the main script, with starts
  // with a wait_for_dom
  ASSERT_THAT(processed_actions2_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kWaitForDom,
            processed_actions2_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunMultipleInterruptInOrder) {
  // All elements exist. The two interrupts should run, in order, then the
  // element should be reported as found.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt1", "interrupt_trigger1");
  SetupInterrupt("interrupt2", "interrupt_trigger2");

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_service_,
                OnGetNextActions(_, _, "payload for interrupt1", _, _))
        .WillOnce(RunOnceCallback<4>(true, ""));
    EXPECT_CALL(mock_service_,
                OnGetNextActions(_, _, "payload for interrupt2", _, _))
        .WillOnce(RunOnceCallback<4>(true, ""));
    EXPECT_CALL(mock_service_,
                OnGetNextActions(_, _, "main script payload", _, _))
        .WillOnce(RunOnceCallback<4>(true, ""));
  }

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt1", SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt2", SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, RunSameInterruptMultipleTimes) {
  // In a main script with three wait_for_dom with allow_interrupt=true...
  ActionsResponseProto interruptible;
  for (int i = 0; i < 3; i++) {
    auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
    wait_action->mutable_wait_until()->add_selectors("element");
    wait_action->set_allow_interrupt(true);
  }
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("script_path"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interruptible)));

  // 'interrupt' with matching preconditions runs exactly three times.
  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  InitInterruptActions(&interrupt_actions, "interrupt");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .Times(3)
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  // All scripts succeed with no more actions.
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptRuns) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto next_interrupt_actions_response;
  next_interrupt_actions_response.set_global_payload(
      "last global payload from interrupt");
  next_interrupt_actions_response.set_script_payload(
      "last payload from interrupt");
  EXPECT_CALL(mock_service_, OnGetNextActions(_, "global payload for interrupt",
                                              "payload for interrupt", _, _))
      .WillOnce(
          RunOnceCallback<4>(true, Serialize(next_interrupt_actions_response)));

  ActionsResponseProto next_main_actions_response;
  next_main_actions_response.set_global_payload(
      "last global payload from main");
  next_main_actions_response.set_script_payload("last payload from main");
  EXPECT_CALL(mock_service_,
              OnGetNextActions(_, "last global payload from interrupt",
                               "main script payload", _, _))
      .WillOnce(
          RunOnceCallback<4>(true, Serialize(next_main_actions_response)));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("last global payload from main", last_global_payload_);
  EXPECT_EQ("last payload from main", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptFails) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_, OnGetNextActions(_, "global payload for interrupt",
                                              "payload for interrupt", _, _))
      .WillOnce(RunOnceCallback<4>(false, ""));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, "global payload for interrupt",
                                              "main script payload", _, _))
      .WillOnce(RunOnceCallback<4>(false, ""));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("global payload for interrupt", last_global_payload_);
  EXPECT_EQ("main script payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfPreconditionsDontMatch) {
  // interrupt_trigger does not exist, but element does, so wait_for_dom will
  // succeed without calling the interrupt.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"interrupt_trigger"})), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_, Not(Contains(Pair(StrEq("interrupt"), _))));
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfNotInterruptible) {
  // The main script has a wait_for_dom, but it is not interruptible.
  ActionsResponseProto interruptible;
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  wait_action->mutable_wait_until()->add_selectors("element");
  // allow_interrupt is not set
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(interruptible)));

  // The interrupt would trigger, since interrupt_trigger exits, but it's not
  // given an opportunity to.
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_, Not(Contains(Pair(StrEq("interrupt"), _))));
}

TEST_F(ScriptExecutorTest, InterruptFailsMainScript) {
  // The interrupt is run and fails. Failure should cascade.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  // The interrupt fails.
  EXPECT_CALL(mock_service_,
              OnGetNextActions(_, _, "payload for interrupt", _, _))
      .WillOnce(RunOnceCallback<4>(false, ""));

  // The main script gets a report of the failure from the interrupt, and fails
  // in turn.
  EXPECT_CALL(
      mock_service_,
      OnGetNextActions(
          _, _, "main script payload",
          ElementsAre(Property(&ProcessedActionProto::status,
                               ProcessedActionStatusProto::INTERRUPT_FAILED)),
          _))
      .WillOnce(RunOnceCallback<4>(false, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_FAILURE)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_FAILURE)));
}

TEST_F(ScriptExecutorTest, InterruptReturnsShutdown) {
  // The interrupt succeeds, but executes the stop action. This should stop the
  // execution of the main script and make it return result.at_end=SHUTDOWN
  SetupInterruptibleScript(kScriptPath, "element");

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_stop();

  // Get interrupt actions
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  // We expect to get result of interrupt action, then result of the main script
  // action.
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, UpdateScriptListGetNext) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  auto* script =
      next_actions_response.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->mutable_chip()->set_text("name");
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, Serialize(next_actions_response)))
      .WillOnce(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
  EXPECT_THAT("name", scripts_update_[0]->handle.chip.text);
}

TEST_F(ScriptExecutorTest, UpdateScriptListShouldNotifyMultipleTimes) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");
  auto* script = actions_response.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->mutable_chip()->set_text("name");
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  script->set_path("path2");
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, Serialize(actions_response)))
      .WillOnce(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_count_, Eq(2));
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT("path2", scripts_update_[0]->handle.path);
}

TEST_F(ScriptExecutorTest, UpdateScriptListFromInterrupt) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  SetupInterruptibleScript(kScriptPath, "element");

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_tell()->set_message("abc");

  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  auto* script = interrupt_actions.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->mutable_chip()->set_text("update_from_interrupt");
  presentation->mutable_precondition();

  // We expect a call from the interrupt which will update the script list and a
  // second call from the interrupt to terminate. Then a call from the main
  // script which will finish without running any actions.
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .Times(3)
      .WillOnce(RunOnceCallback<4>(true, Serialize(interrupt_actions)))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
  EXPECT_THAT("update_from_interrupt", scripts_update_[0]->handle.chip.text);
}

TEST_F(ScriptExecutorTest, RestorePreInterruptStatusMessage) {
  ActionsResponseProto interruptible;
  interruptible.add_actions()->mutable_tell()->set_message(
      "pre-interrupt status");
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  wait_action->mutable_wait_until()->add_selectors("element");
  wait_action->set_allow_interrupt(true);
  EXPECT_CALL(mock_service_, OnGetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interruptible)));

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_tell()->set_message(
      "interrupt status");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  delegate_.SetStatusMessage("pre-run status");
  executor_->Run(executor_callback_.Get());
  EXPECT_EQ("pre-interrupt status", delegate_.GetStatusMessage());
}

TEST_F(ScriptExecutorTest, KeepStatusMessageWhenNotInterrupted) {
  ActionsResponseProto interruptible;
  interruptible.add_actions()->mutable_tell()->set_message(
      "pre-interrupt status");
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  wait_action->mutable_wait_until()->add_selectors("element");
  wait_action->set_allow_interrupt(true);
  EXPECT_CALL(mock_service_, OnGetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interruptible)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<4>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  delegate_.SetStatusMessage("pre-run status");
  executor_->Run(executor_callback_.Get());
  EXPECT_EQ("pre-interrupt status", delegate_.GetStatusMessage());
}

TEST_F(ScriptExecutorTest, PauseWaitForDomWhileNavigating) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->set_timeout_ms(2000);
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // First check does not find the element, wait for dom waits 1s.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  executor_->Run(executor_callback_.Get());

  // Navigation starts while WaitForDom is waiting. The action doesn't fail,
  // even though navigation takes a few seconds longer than the WaitForDom
  // timeout.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  for (int i = 0; i < 5; i++) {
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // The end of navigation un-pauses WaitForDom.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(executor_callback_, Run(_));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StartWaitForDomWhileNavigating) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->set_timeout_ms(2000);
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // Navigation starts before WaitForDom even starts, so the operation starts in
  // a paused state.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  executor_->Run(executor_callback_.Get());

  // The end of navigation un-pauses WaitForDom, which then succeeds
  // immediately.
  EXPECT_CALL(executor_callback_, Run(_));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, ReportErrorAsNavigationError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ true);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());

  // The original error is overwritten; a navigation error is reported.
  EXPECT_EQ(NAVIGATION_ERROR, processed_actions_capture[0].status());
  EXPECT_EQ(OTHER_ACTION_STATUS,
            processed_actions_capture[0].status_details().original_status());
}

TEST_F(ScriptExecutorTest, NavigateWhileRunningInterrupt) {
  SetupInterruptibleScript(kScriptPath, "element");
  RegisterInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto interrupt_actions;
  InitInterruptActions(&interrupt_actions, "interrupt");

  // A load even happens when loading the interrupt scripts, so while the
  // interrupt is being executed. This should not interfere with the WaitForDom
  // action that's running the interrupt.
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(
          DoAll(InvokeWithoutArgs(
                    [this]() { delegate_.UpdateNavigationState(true, false); }),
                RunOnceCallback<5>(true, Serialize(interrupt_actions)),
                InvokeWithoutArgs([this]() {
                  delegate_.UpdateNavigationState(false, false);
                })));

  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions1_capture),
                      RunOnceCallback<4>(true, "")))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions2_capture),
                      RunOnceCallback<4>(true, "")));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
}

TEST_F(ScriptExecutorTest, ReportNavigationErrors) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("a");
  actions_response.add_actions()->mutable_tell()->set_message("b");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ true);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().has_error());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
  EXPECT_TRUE(processed_actions_capture[1].navigation_info().has_error());
}

TEST_F(ScriptExecutorTest, ReportNavigationEnd) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // Navigation starts, before the script is run.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  // WaitForDom waits for navigation to end, then checks for the element, which
  // fails.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  // Checking for the element succeeds on the second try. Waiting avoids
  // depending on the order at which the listeners are called.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_FALSE(processed_actions_capture[0].navigation_info().started());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().ended());
}

TEST_F(ScriptExecutorTest, ReportUnexpectedNavigationStart) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // As the element doesn't exist, WaitForDom returns and waits for 1s.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // Navigation end forces a re-check, which succeeds
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().started());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().unexpected());
}

TEST_F(ScriptExecutorTest, ReportExpectedNavigationStart) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->mutable_wait_until()->add_selectors("element");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // As the element doesn't exist, WaitForDom returns and waits for 1s.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // Navigation end forces a re-check, which succeeds
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[1].navigation_info().started());
  EXPECT_FALSE(processed_actions_capture[1].navigation_info().unexpected());
}

TEST_F(ScriptExecutorTest, WaitForNavigationWithoutExpectation) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // WaitForNavigation returns immediately
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(INVALID_ACTION, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, ExpectNavigation) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // WaitForNavigation waits for navigation to start after expect_navigation
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);
  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
}

TEST_F(ScriptExecutorTest, MultipleWaitForNavigation) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // The first wait_for_navigation waits for the navigation to happen. After
  // that, the other wait_for_navigation return immediately.
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);
  ASSERT_THAT(processed_actions_capture, SizeIs(4));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[2].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[3].status());
}

TEST_F(ScriptExecutorTest, ExpectLaterNavigationIgnoringNavigationInProgress) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // WaitForNavigation waits for navigation to *start* after expect_navigation
  executor_->Run(executor_callback_.Get());

  // This ends the navigation that was in progress when expect_navigation was
  // called. wait_for_navigation should not return, since navigation started
  // after expect_navigation was called.
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  // This starts the new navigation.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // This ends the new navigation. wait_for_navigation returns.
  EXPECT_CALL(executor_callback_, Run(_));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
}

TEST_F(ScriptExecutorTest, WaitForNavigationReportsError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&processed_actions_capture),
                      RunOnceCallback<4>(true, "")));

  // WaitForNavigation waits for navigation to start after expect_navigation
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ true);
  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_EQ(NAVIGATION_ERROR, processed_actions_capture[1].status());
}

TEST_F(ScriptExecutorTest, InterceptUserActions) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_chip()
      ->set_text("done");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  executor_->Run(executor_callback_.Get());
  EXPECT_EQ(AutofillAssistantState::PROMPT, delegate_.GetState());
  ASSERT_NE(nullptr, delegate_.GetUserActions());
  ASSERT_THAT(*delegate_.GetUserActions(), SizeIs(1));

  // The prompt action must finish. We don't bother continuing with the script
  // in this test.
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _));

  (*delegate_.GetUserActions())[0].Call(TriggerContext::CreateEmpty());
  EXPECT_EQ(AutofillAssistantState::RUNNING, delegate_.GetState());
}

TEST_F(ScriptExecutorTest, ReportDirectActionsChoices) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()
      ->mutable_prompt()
      ->add_choices()
      ->mutable_direct_action()
      ->add_names("done");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(SaveArg<3>(&processed_actions_capture));

  auto context = std::make_unique<TriggerContextImpl>();
  context->SetDirectAction(true);
  executor_->Run(executor_callback_.Get());

  ASSERT_NE(nullptr, delegate_.GetUserActions());
  ASSERT_THAT(*delegate_.GetUserActions(), SizeIs(1));
  (*delegate_.GetUserActions())[0].Call(std::move(context));

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_TRUE(processed_actions_capture[0].direct_action());
}

}  // namespace
}  // namespace autofill_assistant
