// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <map>

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

class ScriptExecutorTest : public testing::Test,
                           public ScriptExecutorDelegate,
                           public ScriptExecutor::Listener {
 public:
  void SetUp() override {
    executor_ = std::make_unique<ScriptExecutor>("script path", "", this, this);
    url_ = GURL("http://example.com/");

    // In this test, "tell" actions always succeed and "click" actions always
    // fail. The following makes a click action fail immediately
    ON_CALL(mock_web_controller_, OnElementCheck(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true));
    ON_CALL(mock_web_controller_, OnClickElement(_, _))
        .WillByDefault(RunOnceCallback<1>(false));
    ON_CALL(mock_web_controller_, OnFocusElement(_, _))
        .WillByDefault(RunOnceCallback<1>(true));
    ON_CALL(mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));
    ON_CALL(mock_ui_controller_, ShowOverlay()).WillByDefault(Invoke([this]() {
      overlay_ = true;
    }));
    ON_CALL(mock_ui_controller_, HideOverlay()).WillByDefault(Invoke([this]() {
      overlay_ = false;
    }));
  }

 protected:
  ScriptExecutorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        overlay_(false) {}

  Service* GetService() override { return &mock_service_; }

  UiController* GetUiController() override { return &mock_ui_controller_; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return &memory_; }

  const std::map<std::string, std::string>& GetParameters() override {
    return parameters_;
  }

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }

  void OnServerPayloadChanged(const std::string& server_payload) override {}

  content::WebContents* GetWebContents() override { return nullptr; }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  Script script_;
  ClientMemory memory_;
  StrictMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  NiceMock<MockUiController> mock_ui_controller_;
  std::unique_ptr<ScriptExecutor> executor_;
  std::map<std::string, std::string> parameters_;
  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;
  GURL url_;
  bool overlay_;
};

TEST_F(ScriptExecutorTest, GetActionsFails) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, false),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardParameters) {
  parameters_["param1"] = "value1";
  parameters_["param2"] = "value2";
  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("script path"),
                           AllOf(Contains(Pair("param1", "value1")),
                                 Contains(Pair("param2", "value2"))),
                           _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunOneActionReportAndReturn) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunMultipleActions) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.set_server_payload("payload1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("script path"), _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_server_payload("payload2");
  next_actions_response.add_actions()->mutable_tell()->set_message("3");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&processed_actions1_capture),
                RunOnceCallback<2>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions2_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(1u, processed_actions2_capture.size());
}

TEST_F(ScriptExecutorTest, UnsupportedAction) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions();  // action definition missing

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(UNKNOWN_ACTION_STATUS, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StopAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ResetAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()->mutable_reset();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::RESTART))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, InterruptActionListOnError) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.set_server_payload("payload");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "will pass");
  initial_actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "never run");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_server_payload("payload2");
  next_actions_response.add_actions()->mutable_tell()->set_message(
      "will run after error");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&processed_actions1_capture),
                RunOnceCallback<2>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions2_capture),
                      RunOnceCallback<2>(true, "")));
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
  actions_response.set_server_payload("payload");
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("delayed");
  action->set_action_delay_ms(1000);

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions_capture),
                      RunOnceCallback<2>(true, "")));

  // executor_callback_.Run() not expected to be run just yet, as the action is
  // delayed.
  executor_->Run(executor_callback_.Get());
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());

  // Moving forward in time triggers action execution.
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
}

TEST_F(ScriptExecutorTest, HideDetailsWhenFinished) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");

  *actions_response.add_actions() = click_with_clean_contextual_ui;

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  EXPECT_CALL(mock_ui_controller_, HideDetails());
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, DontHideDetailsIfOtherActionsAreLeft) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");
  *actions_response.add_actions() = click_with_clean_contextual_ui;
  actions_response.add_actions()->mutable_tell()->set_message("Wait no!");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  EXPECT_CALL(mock_ui_controller_, HideDetails()).Times(0);

  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, HideDetailsOnError) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()->mutable_tell()->set_message("Hello");
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));

  EXPECT_CALL(mock_ui_controller_, HideDetails());

  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, HideOverlay) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()->mutable_tell()->set_message("1");
  // focus_element hides the overlay
  actions_response.add_actions()
      ->mutable_focus_element()
      ->mutable_element()
      ->add_selectors("exists");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  EXPECT_CALL(executor_callback_, Run(_));

  overlay_ = true;
  executor_->Run(executor_callback_.Get());
  ASSERT_FALSE(overlay_);
}

TEST_F(ScriptExecutorTest, ShowOverlayAgainAfterHiding) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()
      ->mutable_focus_element()
      ->mutable_element()
      ->add_selectors("exists");
  // tell shows the overlay again, after it's been hidden by focus_element
  actions_response.add_actions()->mutable_tell()->set_message("1");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  EXPECT_CALL(executor_callback_, Run(_));

  overlay_ = true;
  executor_->Run(executor_callback_.Get());
  ASSERT_TRUE(overlay_);
}

}  // namespace
}  // namespace autofill_assistant
