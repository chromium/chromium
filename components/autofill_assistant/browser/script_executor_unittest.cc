// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "net/http/http_status_code.h"
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
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;
using ::testing::WithArgs;

ElementAreaProto MakeElementAreaProto(const std::string& id) {
  Selector touchable_element({id});
  ElementAreaProto area;
  *area.add_touchable()->add_elements() = touchable_element.proto;
  return area;
}

const char* kScriptPath = "script_path";

class ScriptExecutorTest : public testing::Test,
                           public ScriptExecutor::Listener {
 public:
  void SetUp() override {
    delegate_.SetService(&mock_service_);
    delegate_.SetWebController(&mock_web_controller_);
    delegate_.SetCurrentURL(GURL("http://example.com/"));

    TriggerContext::Options options;
    options.experiment_ids = "additional_exp";
    executor_ = std::make_unique<ScriptExecutor>(
        kScriptPath,
        std::make_unique<TriggerContext>(
            std::make_unique<ScriptParameters>(
                base::flat_map<std::string, std::string>{
                    {"additional_param", "additional_param_value"}}),
            options),
        /* global_payload= */ "initial global payload",
        /* script_payload= */ "initial payload",
        /* listener= */ this, &ordered_interrupts_,
        /* delegate= */ &delegate_, /* ui_delegate= */ &ui_delegate_,
        /* is_interrupt_executor= */ false);

    test_util::MockFindAnyElement(mock_web_controller_);
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
    *wait_action->mutable_wait_condition()->mutable_match() =
        ToSelectorProto(element);
    wait_action->set_allow_interrupt(true);
    interruptible.add_actions()->mutable_tell()->set_message(path);
    EXPECT_CALL(mock_service_, GetActions(StrEq(path), _, _, _, _, _))
        .WillRepeatedly(
            RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                               ServiceRequestSender::ResponseInfo{}));
  }

  // Creates an interrupt that contains a tell. It will always succeed.
  void SetupInterrupt(const std::string& path, const std::string& trigger) {
    RegisterInterrupt(path, trigger);

    ActionsResponseProto interrupt_actions;
    InitInterruptActions(&interrupt_actions, path);
    EXPECT_CALL(mock_service_, GetActions(StrEq(path), _, _, _, _, _))
        .WillRepeatedly(
            RunOnceCallback<5>(net::HTTP_OK, Serialize(interrupt_actions),
                               ServiceRequestSender::ResponseInfo{}));
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
    *interrupt_preconditions.mutable_element_condition()->mutable_match() =
        ToSelectorProto(trigger);
    interrupt->precondition =
        ScriptPrecondition::FromProto(path, interrupt_preconditions);

    ordered_interrupts_.emplace_back(std::move(interrupt));
  }

  // task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_environment_;
  FakeScriptExecutorDelegate delegate_;
  FakeScriptExecutorUiDelegate ui_delegate_;
  Script script_;
  StrictMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;

  std::vector<std::unique_ptr<Script>> ordered_interrupts_;
  std::string last_global_payload_;
  std::string last_script_payload_;
  bool should_update_scripts_ = false;
  std::vector<std::unique_ptr<Script>> scripts_update_;
  int scripts_update_count_ = 0;
  std::unique_ptr<ScriptExecutor> executor_;
  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;

  UserData user_data_;
};

TEST_F(ScriptExecutorTest, GetActionsFails) {
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, false),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(&user_data_, executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardParameters) {
  TriggerContext::Options options;
  options.experiment_ids = "exp";
  delegate_.SetTriggerContext(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"param", "value"}}),
      options));
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce([](const std::string& script_path, const GURL& url,
                   const TriggerContext& trigger_context,
                   const std::string& global_payload,
                   const std::string& script_payload,
                   ServiceRequestSender::ResponseCallback callback) {
        // |trigger_context| includes data passed to
        // ScriptExecutor constructor as well as data from the
        // delegate's TriggerContext.
        EXPECT_THAT(trigger_context.GetExperimentIds(),
                    Eq("exp,additional_exp"));

        EXPECT_THAT(
            trigger_context.GetScriptParameters().ToProto(),
            UnorderedElementsAreArray(base::flat_map<std::string, std::string>(
                {{"additional_param", "additional_param_value"},
                 {"param", "value"}})));

        std::move(callback).Run(net::HTTP_OK, /* response= */ "",
                                ServiceRequestSender::ResponseInfo{});
      });

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunOneActionReportAndReturn) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_js_click();  // Invalid.

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(INVALID_ACTION, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].has_run_time_ms());
  EXPECT_GE(processed_actions_capture[0].run_time_ms(), 0);
}

TEST_F(ScriptExecutorTest, RunMultipleActions) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                   Serialize(initial_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("3");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          SaveArg<3>(&processed_actions1_capture),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions2_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(1u, processed_actions2_capture.size());
}

ACTION_P2(Delay, env, delay) {
  env->FastForwardBy(base::Milliseconds(delay));
}

TEST_F(ScriptExecutorTest, ShowsSlowConnectionWarningReplace) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->max_consecutive_slow_roundtrips = 2;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
}

TEST_F(ScriptExecutorTest, ShowsSlowConnectionWarningConcatenate) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "... slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->max_consecutive_slow_roundtrips = 2;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::CONCATENATE;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "1... slow");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
}

TEST_F(ScriptExecutorTest, SlowConnectionWarningTriggersOnlyOnce) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->only_show_connection_warning_once = true;
  client_settings->max_consecutive_slow_roundtrips = 1;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
}

TEST_F(ScriptExecutorTest, SlowConnectionWarningTriggersMultipleTimes) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->only_show_connection_warning_once = false;
  client_settings->only_show_warning_once = false;
  client_settings->max_consecutive_slow_roundtrips = 1;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
}

TEST_F(ScriptExecutorTest, SlowConnectionWarningNotShowingIfNotConsecutive) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->max_consecutive_slow_roundtrips = 2;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(RunOnceCallback<6>(net::HTTP_OK,
                                         Serialize(initial_actions_response),
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_NE(ui_delegate_.GetStatusMessage(), "slow");
}

TEST_F(ScriptExecutorTest, SlowConnectionWarningNotShowingIfOnCompleted) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow";
  client_settings->enable_slow_connection_warnings = true;
  client_settings->max_consecutive_slow_roundtrips = 2;
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<5>(net::HTTP_OK, Serialize(initial_actions_response),
                             ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(initial_actions_response),
                                   ServiceRequestSender::ResponseInfo{}))
      .WillOnce(
          DoAll(Delay(&task_environment_, 600),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_NE(ui_delegate_.GetStatusMessage(), "slow");
}

TEST_F(ScriptExecutorTest, SlowConnectionWarningNotShownIfSlowWebsiteFirst) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow connection";
  client_settings->slow_website_message = "slow website";
  client_settings->enable_slow_website_warnings = true;
  client_settings->warning_delay = base::Milliseconds(1500);
  client_settings->enable_slow_connection_warnings = true;
  client_settings->only_show_warning_once = true;
  client_settings->max_consecutive_slow_roundtrips = 2;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto tell1_waitfordom;
  tell1_waitfordom.add_actions()->mutable_tell()->set_message("1");
  auto* wait_for_dom = tell1_waitfordom.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(
          DoAll(Delay(&task_environment_, 600),
                RunOnceCallback<5>(net::HTTP_OK, Serialize(tell1_waitfordom),
                                   ServiceRequestSender::ResponseInfo{})));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 2000),
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr)))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  ActionsResponseProto tell2;
  tell2.add_actions()->mutable_tell()->set_message("2");
  ActionsResponseProto tell3;
  tell3.add_actions()->mutable_tell()->set_message("3");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(Delay(&task_environment_, 600),
                      RunOnceCallback<6>(net::HTTP_OK, Serialize(tell2),
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(DoAll(Delay(&task_environment_, 600),
                      RunOnceCallback<6>(net::HTTP_OK, Serialize(tell3),
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow website");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "3");
}

TEST_F(ScriptExecutorTest, SlowWebsiteWarningReplace) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_website_message = "slow";
  client_settings->enable_slow_website_warnings = true;
  client_settings->warning_delay = base::Milliseconds(1500);
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("1");
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(Delay(&task_environment_, 2000));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
}

TEST_F(ScriptExecutorTest, SlowWebsiteWarningConcatenate) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_website_message = "... slow";
  client_settings->enable_slow_website_warnings = true;
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::CONCATENATE;
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("1");
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(Delay(&task_environment_, 2000));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "1... slow");
}

TEST_F(ScriptExecutorTest, SlowWebsiteWarningTriggersOnlyOnce) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_website_message = "slow";
  client_settings->enable_slow_website_warnings = true;
  client_settings->only_show_website_warning_once = true;
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("1");
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  auto* second_wait_for_dom =
      next_actions_response.add_actions()->mutable_wait_for_dom();
  *second_wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element2");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 2000),
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr)))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element2"}), _, _))
      .WillOnce(DoAll(Delay(&task_environment_, 2000),
                      RunOnceCallback<2>(
                          ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr)));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(next_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
}

TEST_F(ScriptExecutorTest, SlowWebsiteWarningNotShownIfSlowConnectionFirst) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow connection";
  client_settings->slow_website_message = "slow website";
  client_settings->enable_slow_website_warnings = true;
  client_settings->warning_delay = base::Milliseconds(1500);
  client_settings->enable_slow_connection_warnings = true;
  client_settings->only_show_warning_once = true;
  client_settings->max_consecutive_slow_roundtrips = 1;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto tell1;
  tell1.add_actions()->mutable_tell()->set_message("1");
  ActionsResponseProto tell2_waitfordom;
  tell2_waitfordom.add_actions()->mutable_tell()->set_message("2");
  auto* wait_for_dom = tell2_waitfordom.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  ActionsResponseProto tell3;
  tell3.add_actions()->mutable_tell()->set_message("3");

  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(
          DoAll(Delay(&task_environment_, 600),
                RunOnceCallback<5>(net::HTTP_OK, Serialize(tell1),
                                   ServiceRequestSender::ResponseInfo{})));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 2000),
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr)))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(Delay(&task_environment_, 600),
                RunOnceCallback<6>(net::HTTP_OK, Serialize(tell2_waitfordom),
                                   ServiceRequestSender::ResponseInfo{})))
      .WillOnce(DoAll(Delay(&task_environment_, 600),
                      RunOnceCallback<6>(net::HTTP_OK, Serialize(tell3),
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow connection");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "2");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "3");
}

TEST_F(ScriptExecutorTest, SlowWarningsBothShownIfConfigured) {
  ClientSettings* client_settings = delegate_.GetMutableSettings();
  client_settings->slow_connection_message = "slow connection";
  client_settings->slow_website_message = "slow website";
  client_settings->enable_slow_website_warnings = true;
  client_settings->warning_delay = base::Milliseconds(1500);
  client_settings->enable_slow_connection_warnings = true;
  client_settings->only_show_warning_once = false;
  client_settings->max_consecutive_slow_roundtrips = 1;
  client_settings->slow_roundtrip_threshold = base::Milliseconds(100);
  client_settings->minimum_warning_duration = base::Milliseconds(100);
  client_settings->message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;
  ActionsResponseProto tell1;
  tell1.add_actions()->mutable_tell()->set_message("1");
  ActionsResponseProto tell2_and_waitfordom;
  tell2_and_waitfordom.add_actions()->mutable_tell()->set_message("2");
  auto* wait_for_dom =
      tell2_and_waitfordom.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  ActionsResponseProto tell3;
  tell3.add_actions()->mutable_tell()->set_message("3");

  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(
          DoAll(Delay(&task_environment_, 600),
                RunOnceCallback<5>(net::HTTP_OK, Serialize(tell1),
                                   ServiceRequestSender::ResponseInfo{})));

  // Active check takes longer than warning timeout.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(DoAll(
          Delay(&task_environment_, 2000),
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr)))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          Delay(&task_environment_, 600),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(tell2_and_waitfordom),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(DoAll(Delay(&task_environment_, 600),
                      RunOnceCallback<6>(net::HTTP_OK, Serialize(tell3),
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow connection");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "slow website");
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "3");
}

TEST_F(ScriptExecutorTest, UnsupportedAction) {
  ActionsResponseProto actions_response;
  actions_response.add_actions();  // action definition missing

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(UNSUPPORTED_ACTION, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StopAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(&user_data_, executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, StopClearsUnexecutedActions) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();
  actions_response.add_actions()->mutable_tell()->set_message("should not run");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_NE(ui_delegate_.GetStatusMessage(), "should not run");
  ASSERT_EQ(processed_actions_capture.size(), 1u);
  EXPECT_EQ(processed_actions_capture[0].action(), actions_response.actions(0));
}

TEST_F(ScriptExecutorTest, StopActionGetsExecutedAfterEmptyResponse) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto second_actions_response;
  second_actions_response.add_actions()->mutable_tell()->set_message(
      "tell message");
  std::vector<ProcessedActionProto> second_response_processed_actions_capture;
  std::vector<ProcessedActionProto> third_response_processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      // Second response.
      .WillOnce(DoAll(
          SaveArg<3>(&second_response_processed_actions_capture),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(second_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      // Third response - empty. We only expect the execution to stop after this
      // response.
      .WillOnce(
          DoAll(SaveArg<3>(&third_response_processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(&user_data_, executor_callback_.Get());

  // We expect the actions from the second response to have been executed.
  EXPECT_EQ(ui_delegate_.GetStatusMessage(), "tell message");
  ASSERT_EQ(second_response_processed_actions_capture.size(), 1u);
  EXPECT_EQ(second_response_processed_actions_capture[0].action(),
            actions_response.actions(0));
  ASSERT_EQ(third_response_processed_actions_capture.size(), 1u);
  EXPECT_EQ(third_response_processed_actions_capture[0].action(),
            second_actions_response.actions(0));
}

TEST_F(ScriptExecutorTest, InterruptActionListOnError) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "will pass");
  initial_actions_response.add_actions()->mutable_js_click();  // Invalid.
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "never run");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                   Serialize(initial_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message(
      "will run after error");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          SaveArg<3>(&processed_actions1_capture),
          RunOnceCallback<6>(net::HTTP_OK, Serialize(next_actions_response),
                             ServiceRequestSender::ResponseInfo{})))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions2_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(INVALID_ACTION, processed_actions1_capture[1].status());

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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // executor_callback_.Run() not expected to be run just yet, as the action is
  // delayed.
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  // Moving forward in time triggers action execution.
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(ScriptExecutorTest, ClearDetailsWhenFinished) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");

  *actions_response.add_actions() = click_with_clean_contextual_ui;

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  // empty, but not null
  ui_delegate_.SetDetails(std::make_unique<Details>(), base::TimeDelta());
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_THAT(ui_delegate_.GetDetails(), IsEmpty());
}

TEST_F(ScriptExecutorTest, DontClearDetailsIfOtherActionsAreLeft) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");
  *actions_response.add_actions() = click_with_clean_contextual_ui;
  actions_response.add_actions()->mutable_tell()->set_message("Wait no!");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  // empty, but not null
  ui_delegate_.SetDetails(std::make_unique<Details>(), base::TimeDelta());
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_THAT(ui_delegate_.GetDetails(), Not(IsEmpty()));
}

TEST_F(ScriptExecutorTest, ClearDetailsOnError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello");
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));

  // empty, but not null
  ui_delegate_.SetDetails(std::make_unique<Details>(), base::TimeDelta());
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_THAT(ui_delegate_.GetDetails(), IsEmpty());
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnSuccess) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, GetActions(_, _, _, "initial global payload",
                                        "initial payload", _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_global_payload("last global payload");
  next_actions_response.set_script_payload("last payload");
  EXPECT_CALL(mock_service_, GetNextActions(_, "actions global payload",
                                            "actions payload", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(next_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ("last global payload", last_global_payload_);
  EXPECT_EQ("last payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnError) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, GetActions(_, _, _, "initial global payload",
                                        "initial payload", _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_service_, GetNextActions(_, "actions global payload",
                                            "actions payload", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ("actions global payload", last_global_payload_);
  EXPECT_EQ("actions payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, WaitForDomWaitUntil) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // First check does not find the element, wait for dom waits 1s, then the
  // element is found, and the action succeeds.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(executor_callback_, Run(_));
  task_environment_.FastForwardBy(base::Seconds(1));

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunInterrupt) {
  // All elements exist, so first the interrupt should be run, then the element
  // should be reported as found.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  // Both scripts end after the first set of actions. Capture the results.
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(SaveArg<3>(&processed_actions1_capture),
                      RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions2_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  // The first script to call GetNextActions is the interrupt, which starts
  // with a tell.
  ASSERT_THAT(processed_actions1_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kTell,
            processed_actions1_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());

  // The second script to call GetNextActions is the main script, with starts
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
                GetNextActions(_, _, "payload for interrupt1", _, _, _, _))
        .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                     ServiceRequestSender::ResponseInfo{}));
    EXPECT_CALL(mock_service_,
                GetNextActions(_, _, "payload for interrupt2", _, _, _, _))
        .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                     ServiceRequestSender::ResponseInfo{}));
    EXPECT_CALL(mock_service_,
                GetNextActions(_, _, "main script payload", _, _, _, _))
        .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                     ServiceRequestSender::ResponseInfo{}));
  }

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunSameInterruptMultipleTimes) {
  // In a main script with three wait_for_dom with allow_interrupt=true...
  ActionsResponseProto interruptible;
  for (int i = 0; i < 3; i++) {
    auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
    *wait_action->mutable_wait_condition()->mutable_match() =
        ToSelectorProto("element");
    wait_action->set_allow_interrupt(true);
  }
  EXPECT_CALL(mock_service_, GetActions(StrEq("script_path"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                         ServiceRequestSender::ResponseInfo{}));

  // 'interrupt' with matching preconditions runs exactly three times.
  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  InitInterruptActions(&interrupt_actions, "interrupt");
  EXPECT_CALL(mock_service_, GetActions(StrEq("interrupt"), _, _, _, _, _))
      .Times(3)
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK,
                                         Serialize(interrupt_actions),
                                         ServiceRequestSender::ResponseInfo{}));

  // All scripts succeed with no more actions.
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptRuns) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto next_interrupt_actions_response;
  next_interrupt_actions_response.set_global_payload(
      "last global payload from interrupt");
  next_interrupt_actions_response.set_script_payload(
      "last payload from interrupt");
  EXPECT_CALL(mock_service_,
              GetNextActions(_, "global payload for interrupt",
                             "payload for interrupt", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(next_interrupt_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto next_main_actions_response;
  next_main_actions_response.set_global_payload(
      "last global payload from main");
  next_main_actions_response.set_script_payload("last payload from main");
  EXPECT_CALL(mock_service_,
              GetNextActions(_, "last global payload from interrupt",
                             "main script payload", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(next_main_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ("last global payload from main", last_global_payload_);
  EXPECT_EQ("last payload from main", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptFails) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_,
              GetNextActions(_, "global payload for interrupt",
                             "payload for interrupt", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_service_, GetNextActions(_, "global payload for interrupt",
                                            "main script payload", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ("global payload for interrupt", last_global_payload_);
  EXPECT_EQ("main script payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfPreconditionsDontMatch) {
  // interrupt_trigger does not exist, but element does, so wait_for_dom will
  // succeed without calling the interrupt.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"interrupt_trigger"}), _, _))
      .WillRepeatedly(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  // The script ends after the first set of actions. There is only one call
  // from the main script, running a WaitForDom - none from the interrupt.
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kWaitForDom,
            processed_actions_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfNotInterruptible) {
  // The main script has a wait_for_dom, but it is not interruptible.
  ActionsResponseProto interruptible;
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  *wait_action->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  // allow_interrupt is not set
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                   ServiceRequestSender::ResponseInfo{}));

  // The interrupt would trigger, since interrupt_trigger exits, but it's not
  // given an opportunity to.
  SetupInterrupt("interrupt", "interrupt_trigger");

  // The script ends after the first set of actions. There is only one call
  // from the main script, running a WaitForDom - none from the interrupt.
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kWaitForDom,
            processed_actions_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, InterruptFailsMainScript) {
  // The interrupt is run and fails. Failure should cascade.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  // The interrupt fails.
  EXPECT_CALL(mock_service_,
              GetNextActions(_, _, "payload for interrupt", _, _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  // The main script gets a report of the failure from the interrupt, and fails
  // in turn.
  EXPECT_CALL(mock_service_,
              GetNextActions(_, _, "main script payload",
                             ElementsAre(Property(
                                 &ProcessedActionProto::status,
                                 ProcessedActionStatusProto::INTERRUPT_FAILED)),
                             _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_THAT(ui_delegate_.GetInterruptNotificationHistory(),
              ElementsAre(FakeScriptExecutorUiDelegate::INTERRUPT_STARTED));
}

TEST_F(ScriptExecutorTest, InterruptReturnsShutdown) {
  // The interrupt succeeds, but executes the stop action. This should stop the
  // execution of the main script and make it return result.at_end=SHUTDOWN
  SetupInterruptibleScript(kScriptPath, "element");

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_stop();

  // Get interrupt actions
  EXPECT_CALL(mock_service_, GetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK,
                                         Serialize(interrupt_actions),
                                         ServiceRequestSender::ResponseInfo{}));

  // We expect to get result of interrupt action, then result of the main script
  // action.
  EXPECT_CALL(mock_service_, GetNextActions)
      .Times(2)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_THAT(ui_delegate_.GetInterruptNotificationHistory(),
              ElementsAre(FakeScriptExecutorUiDelegate::INTERRUPT_STARTED));
}

TEST_F(ScriptExecutorTest, RunInterruptDuringPrompt) {
  RegisterInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto interrupt_actions;
  InitInterruptActions(&interrupt_actions, "interrupt");
  ElementAreaProto interrupt_area =
      MakeElementAreaProto(/* id = */ "interrupt_area");
  *interrupt_actions.add_actions()
       ->mutable_set_touchable_area()
       ->mutable_element_area() = interrupt_area;
  auto* interrupt_prompt = interrupt_actions.add_actions()->mutable_prompt();
  *interrupt_prompt->add_choices()
       ->mutable_auto_select_when()
       ->mutable_match() = ToSelectorProto("end_prompt");

  EXPECT_CALL(mock_service_, GetActions("interrupt", _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK,
                                         Serialize(interrupt_actions),
                                         ServiceRequestSender::ResponseInfo{}));

  // Main script has a prompt with an "auto_select" element. This functions very
  // much like a WaitForDom, except for the UI changes triggered by the switches
  // between PROMPT and RUNNING states.
  ActionsResponseProto interruptible;
  ElementAreaProto interruptible_area =
      MakeElementAreaProto(/* id = */ "interruptible_area");
  *interruptible.add_actions()
       ->mutable_set_touchable_area()
       ->mutable_element_area() = interruptible_area;
  auto* prompt_action = interruptible.add_actions()->mutable_prompt();
  prompt_action->set_allow_interrupt(true);
  *prompt_action->add_choices()->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("end_prompt");
  interruptible.add_actions()->mutable_tell()->set_message("done");
  EXPECT_CALL(mock_service_, GetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"interrupt_trigger"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }))
      .WillRepeatedly(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"end_prompt"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  // Expected scenario:
  // - show prompt (enter PROMPT state)
  // - notice interrupt_trigger element
  // - run interrupt (enter RUNNING state)
  // - show the interrupt's prompt (enter PROMPT state)
  // - the interrupt finishes (enter RUNNING state)
  // - show prompt again (enter PROMPT state)
  // - notice end_prompt element
  // - end prompt, continue main script (enter RUNNING state)
  // - run tell, which sets message to "done"
  EXPECT_THAT(
      delegate_.GetStateHistory(),
      ElementsAre(
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING,
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING,
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING));
  // Expected scenario:
  // - the main script's SetTouchableArea sets |interruptible_area|
  // - the interrupt starts
  // - the interrupt's SetTouchableArea sets |interrupt_area|
  // - the area is cleaned up at the end of the interrupt's prompt
  // - when the main script resumes, we restore |interruptible_area|
  // - the area is cleaned up again at the end of the main script's prompt
  EXPECT_THAT(
      delegate_.GetTouchableElementAreaHistory(),
      ElementsAre(interruptible_area, interrupt_area,
                  ElementAreaProto::default_instance(), interruptible_area,
                  ElementAreaProto::default_instance()));
  EXPECT_EQ("done", ui_delegate_.GetStatusMessage());
  EXPECT_THAT(ui_delegate_.GetInterruptNotificationHistory(),
              ElementsAre(FakeScriptExecutorUiDelegate::INTERRUPT_STARTED,
                          FakeScriptExecutorUiDelegate::INTERRUPT_FINISHED));
}

TEST_F(ScriptExecutorTest, RunPromptInBrowseMode) {
  ActionsResponseProto actions_response;
  auto* prompt = actions_response.add_actions()->mutable_prompt();
  prompt->add_choices()->mutable_chip()->set_text("done");
  prompt->set_browse_mode(true);

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(AutofillAssistantState::BROWSE, delegate_.GetState());
}

TEST_F(ScriptExecutorTest, RunPromptInPromptMode) {
  ActionsResponseProto actions_response;
  auto* prompt = actions_response.add_actions()->mutable_prompt();
  prompt->add_choices()->mutable_chip()->set_text("done");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(AutofillAssistantState::PROMPT, delegate_.GetState());
}

TEST_F(ScriptExecutorTest, RunInterruptMultipleTimesDuringPrompt) {
  SetupInterrupt("interrupt", "interrupt_trigger");

  // Main script has a prompt with an "auto_select" element. This functions very
  // much like a WaitForDom, except for the UI changes triggered by the switches
  // between PROMPT and RUNNING states.
  ActionsResponseProto interruptible;
  auto* prompt_action = interruptible.add_actions()->mutable_prompt();
  prompt_action->set_allow_interrupt(true);
  *prompt_action->add_choices()->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("end_prompt");
  EXPECT_CALL(mock_service_, GetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                         ServiceRequestSender::ResponseInfo{}));

  // interrupt_trigger goes away and come back, which means that the interrupt
  // will be run twice.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"interrupt_trigger"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }))
      .WillRepeatedly(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  // It takes a several rounds for end_prompt to appear, which gives time for
  // the interrupt to run.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"end_prompt"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
  for (int try_count = 0; try_count < 10; try_count++) {
    task_environment_.FastForwardBy(base::Milliseconds(1000));
  }

  EXPECT_THAT(
      delegate_.GetStateHistory(),
      ElementsAre(
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING,
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING,
          AutofillAssistantState::PROMPT, AutofillAssistantState::RUNNING));
  EXPECT_THAT(ui_delegate_.GetInterruptNotificationHistory(),
              ElementsAre(FakeScriptExecutorUiDelegate::INTERRUPT_STARTED,
                          FakeScriptExecutorUiDelegate::INTERRUPT_FINISHED,
                          FakeScriptExecutorUiDelegate::INTERRUPT_STARTED,
                          FakeScriptExecutorUiDelegate::INTERRUPT_FINISHED));
}

TEST_F(ScriptExecutorTest, UpdateScriptListGetNext) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                   Serialize(initial_actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  auto* script =
      next_actions_response.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   Serialize(next_actions_response),
                                   ServiceRequestSender::ResponseInfo{}))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
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
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, GetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  script->set_path("path2");
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

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

  EXPECT_CALL(mock_service_, GetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(interrupt_actions),
                                   ServiceRequestSender::ResponseInfo{}));

  auto* script = interrupt_actions.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->mutable_precondition();

  // We expect a call from the interrupt which will update the script list and a
  // second call from the interrupt to terminate. Then a call from the main
  // script which will finish without running any actions.
  EXPECT_CALL(mock_service_, GetNextActions)
      .Times(3)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, Serialize(interrupt_actions),
                                   ServiceRequestSender::ResponseInfo{}))
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
}

TEST_F(ScriptExecutorTest, RestorePreInterruptStatusMessage) {
  ActionsResponseProto interruptible;
  interruptible.add_actions()->mutable_tell()->set_message(
      "pre-interrupt status");
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  *wait_action->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  wait_action->set_allow_interrupt(true);
  EXPECT_CALL(mock_service_, GetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                         ServiceRequestSender::ResponseInfo{}));

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_tell()->set_message(
      "interrupt status");
  EXPECT_CALL(mock_service_, GetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK,
                                         Serialize(interrupt_actions),
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  ui_delegate_.SetStatusMessage("pre-run status");
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ("pre-interrupt status", ui_delegate_.GetStatusMessage());
}

TEST_F(ScriptExecutorTest, KeepStatusMessageWhenNotInterrupted) {
  ActionsResponseProto interruptible;
  interruptible.add_actions()->mutable_tell()->set_message(
      "pre-interrupt status");
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  *wait_action->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");
  wait_action->set_allow_interrupt(true);
  EXPECT_CALL(mock_service_, GetActions(kScriptPath, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, Serialize(interruptible),
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  ui_delegate_.SetStatusMessage("pre-run status");
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ("pre-interrupt status", ui_delegate_.GetStatusMessage());
}

#if defined(ADDRESS_SANITIZER)
// This test fails on ASAN: https://crbug.com/1315701
#define MAYBE_PauseWaitForDomWhileNavigating \
  DISABLED_PauseWaitForDomWhileNavigating
#else
#define MAYBE_PauseWaitForDomWhileNavigating PauseWaitForDomWhileNavigating
#endif
TEST_F(ScriptExecutorTest, MAYBE_PauseWaitForDomWhileNavigating) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->set_timeout_ms(2000);
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // First check does not find the element, wait for dom waits 1s.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  executor_->Run(&user_data_, executor_callback_.Get());

  // Navigation starts while WaitForDom is waiting. The action doesn't fail,
  // even though navigation takes a few seconds longer than the WaitForDom
  // timeout.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  for (int i = 0; i < 5; i++) {
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // The end of navigation un-pauses WaitForDom.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(executor_callback_, Run(_));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StartWaitForDomWhileNavigating) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  wait_for_dom->set_timeout_ms(2000);
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // Navigation starts before WaitForDom starts. WaitForDom does not wait and
  // completes successfully.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  // Navigation finishes after the WaitForDom has finished.
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_FALSE(processed_actions_capture[0].navigation_info().started());
  EXPECT_FALSE(processed_actions_capture[0].navigation_info().ended());
}

TEST_F(ScriptExecutorTest, NavigateWhileRunningInterrupt) {
  SetupInterruptibleScript(kScriptPath, "element");
  RegisterInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto interrupt_actions;
  InitInterruptActions(&interrupt_actions, "interrupt");

  // A load even happens when loading the interrupt scripts, so while the
  // interrupt is being executed. This should not interfere with the WaitForDom
  // action that's running the interrupt.
  EXPECT_CALL(mock_service_, GetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(WithArgs<5>([this, interrupt_actions](auto callback) {
        delegate_.UpdateNavigationState(true, false);
        std::move(callback).Run(net::HTTP_OK, Serialize(interrupt_actions),
                                ServiceRequestSender::ResponseInfo{});
        delegate_.UpdateNavigationState(false, false);
      }));

  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(SaveArg<3>(&processed_actions1_capture),
                      RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions2_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
}

TEST_F(ScriptExecutorTest, ReportNavigationErrors) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("a");
  actions_response.add_actions()->mutable_tell()->set_message("b");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ true);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().has_error());
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[1].status());
  EXPECT_TRUE(processed_actions_capture[1].navigation_info().has_error());
}

TEST_F(ScriptExecutorTest, ReportNavigationEnd) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // WaitForDom does NOT wait for navigation to end, it immediately checks for
  // the element, which fails.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  // Navigation starts before the script is run.
  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  // Checking for the element succeeds on the second try.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  task_environment_.FastForwardBy(base::Seconds(1));

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_FALSE(processed_actions_capture[0].navigation_info().started());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().ended());
}

#if defined(ADDRESS_SANITIZER)
// This test fails on ASAN: https://crbug.com/1315701
#define MAYBE_ReportUnexpectedNavigationStart \
  DISABLED_ReportUnexpectedNavigationStart
#else
#define MAYBE_ReportUnexpectedNavigationStart ReportUnexpectedNavigationStart
#endif
TEST_F(ScriptExecutorTest, MAYBE_ReportUnexpectedNavigationStart) {
  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // As the element doesn't exist, WaitForDom returns and waits for 1s.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // Navigation end forces a re-check, which succeeds
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  delegate_.UpdateNavigationState(/* navigating= */ false, /* error= */ false);

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(ACTION_APPLIED, processed_actions_capture[0].status());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().started());
  EXPECT_TRUE(processed_actions_capture[0].navigation_info().unexpected());
}

#if defined(ADDRESS_SANITIZER)
// This test fails on ASAN: https://crbug.com/1315701
#define MAYBE_ReportExpectedNavigationStart \
  DISABLED_ReportExpectedNavigationStart
#else
#define MAYBE_ReportExpectedNavigationStart ReportExpectedNavigationStart
#endif
TEST_F(ScriptExecutorTest, MAYBE_ReportExpectedNavigationStart) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // As the element doesn't exist, WaitForDom returns and waits for 1s.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // Navigation end forces a re-check, which succeeds
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // WaitForNavigation returns immediately
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(INVALID_ACTION, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, ExpectNavigation) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_expect_navigation();
  actions_response.add_actions()->mutable_wait_for_navigation();

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // WaitForNavigation waits for navigation to start after expect_navigation
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // The first wait_for_navigation waits for the navigation to happen. After
  // that, the other wait_for_navigation return immediately.
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  delegate_.UpdateNavigationState(/* navigating= */ true, /* error= */ false);

  // WaitForNavigation waits for navigation to *start* after expect_navigation
  executor_->Run(&user_data_, executor_callback_.Get());

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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));

  // WaitForNavigation waits for navigation to start after expect_navigation
  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(&user_data_, executor_callback_.Get());

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

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));

  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_EQ(AutofillAssistantState::PROMPT, delegate_.GetState());
  ASSERT_NE(nullptr, ui_delegate_.GetUserActions());
  ASSERT_THAT(*ui_delegate_.GetUserActions(), SizeIs(1));

  // The prompt action must finish. We don't bother continuing with the script
  // in this test.
  EXPECT_CALL(mock_service_, GetNextActions);

  (*ui_delegate_.GetUserActions())[0].RunCallback();
  EXPECT_EQ(AutofillAssistantState::RUNNING, delegate_.GetState());
}

TEST_F(ScriptExecutorTest, RoundtripTimingStats) {
  ActionsResponseProto actions_response;
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("1");
  action->set_action_delay_ms(1000);
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(
          DoAll(Delay(&task_environment_, 200),
                RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{})));

  RoundtripTimingStats timing_stats;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<4>(&timing_stats),
                RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                   ServiceRequestSender::ResponseInfo{})));
  executor_->Run(&user_data_, executor_callback_.Get());
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  // Moving forward in time triggers action execution.

  EXPECT_EQ(200, timing_stats.roundtrip_time_ms());
  EXPECT_EQ(1000, timing_stats.client_time_ms());
}

TEST_F(ScriptExecutorTest, RoundtripNetworkStatsForSuccessfulRoundtrip) {
  ActionsResponseProto actions_response;
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("Hello world");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_OK, Serialize(actions_response),
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 76}));

  RoundtripNetworkStats captured_network_stats;
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          SaveArg<5>(&captured_network_stats),
          [&]() {
            EXPECT_EQ(captured_network_stats,
                      delegate_.GetRoundtripNetworkStats());
          },
          RunOnceCallback<6>(
              net::HTTP_OK, "",
              ServiceRequestSender::ResponseInfo{.encoded_body_length = 13})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(captured_network_stats.roundtrip_encoded_body_size_bytes(), 76);
  EXPECT_EQ(static_cast<size_t>(
                captured_network_stats.roundtrip_decoded_body_size_bytes()),
            Serialize(actions_response).size());
  ASSERT_EQ(captured_network_stats.action_stats().size(), 1);
  EXPECT_EQ(captured_network_stats.action_stats(0).action_info_case(), 11);
  EXPECT_EQ(static_cast<size_t>(
                captured_network_stats.action_stats(0).decoded_size_bytes()),
            Serialize(*action).size());

  // Roundtrip accumulation is more comprehensively tested in the controller,
  // this is just to make sure that GetActions and GetNextActions are both
  // reported to the delegate.
  EXPECT_EQ(
      delegate_.GetRoundtripNetworkStats().roundtrip_encoded_body_size_bytes(),
      13);
}

TEST_F(ScriptExecutorTest, RoundtripNetworkStatsForFailedRoundtrip) {
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_UNAUTHORIZED, "",
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 12}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(
      delegate_.GetRoundtripNetworkStats().roundtrip_encoded_body_size_bytes(),
      12);
  EXPECT_EQ(
      delegate_.GetRoundtripNetworkStats().roundtrip_decoded_body_size_bytes(),
      0);
  EXPECT_EQ(delegate_.GetRoundtripNetworkStats().num_roundtrips(), 1);
  EXPECT_EQ(delegate_.GetRoundtripNetworkStats().action_stats().size(), 0);
}

TEST_F(ScriptExecutorTest, RoundtripNetworkStatsForParsingError) {
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(
          net::HTTP_OK, "\xff\xff\xff not a valid proto, 36 bytes long",
          ServiceRequestSender::ResponseInfo{.encoded_body_length = 12}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(&user_data_, executor_callback_.Get());

  EXPECT_EQ(
      delegate_.GetRoundtripNetworkStats().roundtrip_encoded_body_size_bytes(),
      12);
  EXPECT_EQ(
      delegate_.GetRoundtripNetworkStats().roundtrip_decoded_body_size_bytes(),
      36);
  EXPECT_EQ(delegate_.GetRoundtripNetworkStats().num_roundtrips(), 1);
  EXPECT_EQ(delegate_.GetRoundtripNetworkStats().action_stats().size(), 0);
}

TEST_F(ScriptExecutorTest, ClearPersistentUiOnError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_UNAUTHORIZED, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));

  // empty, but not null
  ui_delegate_.SetPersistentGenericUi(
      std::make_unique<GenericUserInterfaceProto>(), base::DoNothing());
  ASSERT_NE(nullptr, ui_delegate_.GetPersistentGenericUi());
  executor_->Run(&user_data_, executor_callback_.Get());
  ASSERT_EQ(nullptr, ui_delegate_.GetPersistentGenericUi());
}

TEST_F(ScriptExecutorTest, SetGenericUi) {
  base::MockCallback<
      base::OnceCallback<void(const autofill_assistant::ClientStatus&)>>
      end_action_callback;
  base::MockCallback<
      base::OnceCallback<void(const autofill_assistant::ClientStatus&)>>
      view_inflation_finished_callback;
  base::MockCallback<
      base::RepeatingCallback<void(const RequestBackendDataProto&)>>
      request_backend_data_callback;
  base::MockCallback<
      base::RepeatingCallback<void(const ShowAccountScreenProto&)>>
      show_account_screen_callback;
  ui_delegate_.SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(), end_action_callback.Get(),
      view_inflation_finished_callback.Get(),
      request_backend_data_callback.Get(), show_account_screen_callback.Get());
  ASSERT_NE(nullptr, ui_delegate_.GetGenericUi());
  ASSERT_FALSE(ui_delegate_.GetEndActionCallback().is_null());
  ASSERT_FALSE(ui_delegate_.GetViewInflationFinishedCallback().is_null());
  ASSERT_FALSE(ui_delegate_.GetRequestBackendDataCallback().is_null());
  ASSERT_FALSE(ui_delegate_.GetShowAccountScreenCallback().is_null());
}

TEST_F(ScriptExecutorTest, SetPersistentGenericUi) {
  base::MockCallback<
      base::OnceCallback<void(const autofill_assistant::ClientStatus&)>>
      view_inflation_finished_callback;
  ui_delegate_.SetPersistentGenericUi(
      std::make_unique<GenericUserInterfaceProto>(),
      view_inflation_finished_callback.Get());
  ASSERT_NE(nullptr, ui_delegate_.GetPersistentGenericUi());
  ASSERT_FALSE(ui_delegate_.GetViewInflationFinishedCallback().is_null());
}

TEST_F(ScriptExecutorTest, SetCollectUserDataUiState) {
  executor_->SetCollectUserDataUiState(true,
                                       UserDataEventField::SHIPPING_EVENT);
  EXPECT_EQ(ui_delegate_.GetCollectUserDataUiLoadingField(),
            UserDataEventField::SHIPPING_EVENT);
}

TEST_F(ScriptExecutorTest, RequestUserData) {
  EXPECT_CALL(mock_service_, GetUserData)
      .WillOnce(RunOnceCallback<3>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));

  base::MockCallback<
      base::OnceCallback<void(bool, const GetUserDataResponseProto&)>>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(true, _));
  executor_->RequestUserData(CollectUserDataOptions(), mock_callback.Get());
  EXPECT_THAT(delegate_.GetStateHistory(),
              ElementsAre(AutofillAssistantState::RUNNING,
                          AutofillAssistantState::PROMPT));
}

TEST_F(ScriptExecutorTest, ShowAccountScreen) {
  ShowAccountScreenProto proto;
  proto.set_gms_account_intent_screen_id(4);
  executor_->ShowAccountScreen(proto, "abc@xyz.com");

  EXPECT_EQ(ui_delegate_.GetUserEmail(), "abc@xyz.com");
  EXPECT_EQ(
      ui_delegate_.GetShowAccountScreenProto().gms_account_intent_screen_id(),
      4);
}

TEST_F(ScriptExecutorTest, CollectUserData) {
  // Ui has been disabled while loading.
  ui_delegate_.SetCollectUserDataUiState(/* loading= */ true,
                                         UserDataEventField::SHIPPING_EVENT);
  EXPECT_EQ(ui_delegate_.GetCollectUserDataUiLoadingField(),
            UserDataEventField::SHIPPING_EVENT);

  CollectUserDataOptions options;
  executor_->CollectUserData(&options);

  EXPECT_TRUE(options.confirm_callback);
  EXPECT_TRUE(options.additional_actions_callback);
  EXPECT_TRUE(options.terms_link_callback);
  EXPECT_EQ(ui_delegate_.GetOptions(), &options);
  EXPECT_EQ(ui_delegate_.GetCollectUserDataUiLoadingField(),
            UserDataEventField::NONE);
}

TEST_F(ScriptExecutorTest, ShowQrCodeScanUi) {
  executor_->ShowQrCodeScanUi(
      std::make_unique<PromptQrCodeScanProto>(PromptQrCodeScanProto()),
      base::DoNothing());
  EXPECT_TRUE(ui_delegate_.IsShowingQrCodeScanUi());

  executor_->ClearQrCodeScanUi();
  EXPECT_FALSE(ui_delegate_.IsShowingQrCodeScanUi());
}

TEST_F(ScriptExecutorTest, MustUseBackendData) {
  delegate_.SetMustUseBackendData(true);
  EXPECT_TRUE(executor_->MustUseBackendData());

  delegate_.SetMustUseBackendData(false);
  EXPECT_FALSE(executor_->MustUseBackendData());
}

TEST_F(ScriptExecutorTest, ExternalActionDoesNotApplyTouchableArea) {
  ActionsResponseProto actions_response;
  ElementAreaProto area = MakeElementAreaProto("#area");
  *actions_response.add_actions()
       ->mutable_set_touchable_area()
       ->mutable_element_area() = area;
  actions_response.add_actions()->mutable_external_action()->mutable_info();

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  executor_->Run(&user_data_, executor_callback_.Get());
  // The touchable area was never applied.
  EXPECT_THAT(delegate_.GetTouchableElementAreaHistory(), IsEmpty());
  // The delegate never entered prompt.
  EXPECT_THAT(delegate_.GetStateHistory(), IsEmpty());
}

TEST_F(ScriptExecutorTest, ExternalActionDoesNotConsumeTouchableArea) {
  ActionsResponseProto actions_response;
  ElementAreaProto area = MakeElementAreaProto("#area");
  *actions_response.add_actions()
       ->mutable_set_touchable_area()
       ->mutable_element_area() = area;
  actions_response.add_actions()->mutable_external_action()->mutable_info();
  auto* prompt_action = actions_response.add_actions()->mutable_prompt();
  *prompt_action->add_choices()->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("end_prompt");

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"end_prompt"}), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(),
                                   std::make_unique<ElementFinderResult>()));
  executor_->Run(&user_data_, executor_callback_.Get());
  // Since the ExternalAction did not consume the touchable area, the following
  // prompt action was able to apply it.
  EXPECT_THAT(delegate_.GetTouchableElementAreaHistory(),
              ElementsAre(area, ElementAreaProto::default_instance()));
}

TEST_F(ScriptExecutorTest, ExternalActionAppliesAndRestoresTouchableArea) {
  ActionsResponseProto actions_response;
  ElementAreaProto area = MakeElementAreaProto("#area");
  *actions_response.add_actions()
       ->mutable_set_touchable_area()
       ->mutable_element_area() = area;
  auto* external_action =
      actions_response.add_actions()->mutable_external_action();
  external_action->mutable_info();
  external_action->set_show_touchable_area(true);

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  executor_->Run(&user_data_, executor_callback_.Get());
  // The touchable area was applied at the start of the ExternalAction and
  // restored at the end of it.
  EXPECT_THAT(delegate_.GetTouchableElementAreaHistory(),
              ElementsAre(area, ElementAreaProto::default_instance()));
  EXPECT_THAT(delegate_.GetStateHistory(),
              ElementsAre(AutofillAssistantState::PROMPT,
                          AutofillAssistantState::RUNNING));
}

TEST_F(ScriptExecutorTest,
       TestDelegateIsCalledForExtractValuesFromSingleTagXml) {
  EXPECT_EQ(executor_->ExtractValuesFromSingleTagXml("some_xml", {"some_key"}),
            (const std::vector<std::string>){});
}

TEST_F(ScriptExecutorTest, TestDelegateIsCalledForIsXmlSigned) {
  EXPECT_EQ(executor_->IsXmlSigned("some_xml"), true);
}

TEST_F(ScriptExecutorTest, ReportProgress) {
  EXPECT_CALL(mock_service_, ReportProgress)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));

  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));

  std::string payload = "payload";
  executor_->ReportProgress(payload, mock_callback.Get());
}

TEST_F(ScriptExecutorTest, ReportProgressApplied) {
  ActionsResponseProto actions_response;
  *actions_response.add_actions()
       ->mutable_report_progress()
       ->mutable_payload() = "payload";

  EXPECT_CALL(mock_service_, GetActions)
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, ReportProgress)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(&user_data_, executor_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
