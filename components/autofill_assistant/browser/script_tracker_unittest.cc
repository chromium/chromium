// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::WithArgs;

class ScriptTrackerTest : public testing::Test, public ScriptTracker::Listener {
 public:
  void SetUp() override {
    delegate_.SetCurrentURL(GURL("http://www.example.com/"));

    ON_CALL(mock_web_controller_, FindElement(Selector({"exists"}), _, _))
        .WillByDefault(WithArgs<2>([](auto&& callback) {
          std::move(callback).Run(OkClientStatus(),
                                  std::make_unique<ElementFinderResult>());
        }));
    ON_CALL(mock_web_controller_,
            FindElement(Selector({"does_not_exist"}), _, _))
        .WillByDefault(RunOnceCallback<2>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

    // Scripts run, but have no actions.
    ON_CALL(mock_service_, GetActions)
        .WillByDefault(RunOnceCallback<5>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));
  }

 protected:
  ScriptTrackerTest()
      : no_runnable_scripts_anymore_(0),
        runnable_scripts_changed_(0),
        tracker_(&delegate_, &ui_delegate_, /* listener=*/this) {
    delegate_.SetService(&mock_service_);
    delegate_.SetWebController(&mock_web_controller_);
  }

  // Overrides ScriptTracker::Listener
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override {
    runnable_scripts_changed_++;
    runnable_scripts_ = runnable_scripts;
  }

  void OnNoRunnableScriptsForPage() override { no_runnable_scripts_anymore_++; }

  void SetScripts() {
    std::vector<std::unique_ptr<Script>> scripts;
    for (const auto& script_proto : scripts_proto_) {
      ProtocolUtils::AddScript(*script_proto, &scripts);
    }
    tracker_.SetScripts(std::move(scripts));
  }

  void SetAndCheckScripts() {
    SetScripts();
    tracker_.CheckScripts();
  }

  SupportedScriptProto* AddScript() {
    scripts_proto_.emplace_back(new SupportedScriptProto);
    return scripts_proto_.back().get();
  }

  void InitScriptProto(SupportedScriptProto* script,
                       const std::string& path,
                       const std::string& selector,
                       const std::string& direct_action_name) {
    script->set_path(path);
    if (!selector.empty()) {
      *script->mutable_presentation()
           ->mutable_precondition()
           ->mutable_element_condition()
           ->mutable_match() = ToSelectorProto(selector);
    }
    if (!direct_action_name.empty()) {
      script->mutable_presentation()->mutable_direct_action()->add_names(
          direct_action_name);
    }
  }

  void InitScriptProto(SupportedScriptProto* script,
                       const std::string& path,
                       const std::string& selector) {
    InitScriptProto(script, path, selector, "");
  }

  const std::vector<ScriptHandle>& runnable_scripts() {
    return runnable_scripts_;
  }

  const std::vector<std::string> runnable_script_paths() {
    std::vector<std::string> paths;
    for (const auto& handle : runnable_scripts_) {
      paths.emplace_back(handle.path);
    }
    return paths;
  }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  // task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_environment_;

  GURL url_;
  NiceMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;

  // Number of times NoRunnableScriptsAnymore was called.
  int no_runnable_scripts_anymore_;
  // Number of times OnRunnableScriptsChanged was called.
  int runnable_scripts_changed_;
  std::vector<ScriptHandle> runnable_scripts_;
  FakeScriptExecutorDelegate delegate_;
  FakeScriptExecutorUiDelegate ui_delegate_;
  ScriptTracker tracker_;
  std::vector<std::unique_ptr<SupportedScriptProto>> scripts_proto_;
  UserData user_data_;
};

TEST_F(ScriptTrackerTest, NoScripts) {
  tracker_.SetScripts({});
  tracker_.CheckScripts();
  EXPECT_THAT(runnable_scripts(), IsEmpty());
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, SomeRunnableScripts) {
  InitScriptProto(AddScript(), "not runnable path", "does_not_exist",
                  "direct_action_name");
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, DoNotReportInterruptsAsRunnable) {
  // The interrupt's preconditions would all be met, but it won't be reported as
  // runnable since it's an interrupt.
  auto* interrupt = AddScript();
  InitScriptProto(interrupt, "path1", "exists");
  interrupt->mutable_presentation()->set_interrupt(true);

  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, OrderScriptsByPriority) {
  SupportedScriptProto* script_a = AddScript();
  InitScriptProto(script_a, "a", "", "name_a");
  script_a->mutable_presentation()->set_priority(2);

  SupportedScriptProto* script_b = AddScript();
  InitScriptProto(script_b, "b", "", "name_b");
  script_b->mutable_presentation()->set_priority(3);

  SupportedScriptProto* script_c = AddScript();
  InitScriptProto(script_c, "c", "", "name_c");
  script_c->mutable_presentation()->set_priority(1);

  SetAndCheckScripts();

  ASSERT_THAT(runnable_script_paths(), ElementsAre("c", "a", "b"));
}

TEST_F(ScriptTrackerTest, NewScriptChangesNothing) {
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, NewScriptClearsRunnable) {
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, NewScriptAddsRunnable) {
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  InitScriptProto(AddScript(), "new runnable path", "exists",
                  "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(2));
}

TEST_F(ScriptTrackerTest, NewScriptChangesRunnable) {
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  InitScriptProto(AddScript(), "new runnable path", "exists",
                  "direct_action_name");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, CheckScriptsAfterDOMChange) {
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"maybe_exists"}), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  InitScriptProto(AddScript(), "script path", "maybe_exists",
                  "direct_action_name");
  SetAndCheckScripts();
  // No scripts are runnable.
  EXPECT_THAT(runnable_scripts(), IsEmpty());
  // DOM has changed; OnElementExists now returns truthy.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"maybe_exists"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  tracker_.CheckScripts();
  // The script can now run
  ASSERT_THAT(runnable_script_paths(), ElementsAre("script path"));
}

TEST_F(ScriptTrackerTest, UpdateScriptList) {
  // 1. Initialize runnable scripts with a single valid script.
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the action and trigger a script list update.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update path", "exists", "direct_action_name");
  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update path 2", "exists", "direct_action_name");

  EXPECT_CALL(mock_service_, GetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", &user_data_,
                         std::make_unique<TriggerContext>(),
                         execute_callback.Get());
  tracker_.CheckScripts();

  // 3. Verify that the runnable scripts have changed to the updated list.
  EXPECT_EQ(2, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(2));
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

TEST_F(ScriptTrackerTest, UpdateScriptListFromInterrupt) {
  // 1. Initialize runnable scripts with a single valid interrupt script.
  InitScriptProto(AddScript(), "runnable path", "exists", "direct_action_name");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the interrupt action and trigger a script list update from an
  // interrupt.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update path", "exists", "direct_action_name");
  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update path 2", "exists", "direct_action_name");

  EXPECT_CALL(mock_service_, GetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", &user_data_,
                         std::make_unique<TriggerContext>(),
                         execute_callback.Get());
  tracker_.CheckScripts();

  // 3. Verify that the runnable scripts have changed to the updated list.
  EXPECT_EQ(2, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(2));
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

TEST_F(ScriptTrackerTest, UpdateInterruptList) {
  // The first GetActions response from "main" updates the set of scripts, which
  // includes "interrupt". "interrupt" is then executed from the wait_for_dom.

  InitScriptProto(AddScript(), "main", "exists");
  SetScripts();

  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("exists");
  wait_for_dom->set_allow_interrupt(true);

  SupportedScriptProto* interrupt_proto =
      actions_response.mutable_update_script_list()->add_scripts();
  InitScriptProto(interrupt_proto, "interrupt", "exists");
  interrupt_proto->mutable_presentation()->set_interrupt(true);

  EXPECT_CALL(mock_service_, GetActions("main", _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, "",
                                         ServiceRequestSender::ResponseInfo{}));

  ActionsResponseProto actions_interrupt;
  actions_response.set_script_payload("from interrupt");
  actions_response.add_actions()->mutable_tell()->set_message("interrupt");

  EXPECT_CALL(mock_service_, GetActions("interrupt", _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_interrupt),
                                   ServiceRequestSender::ResponseInfo{}));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("main", &user_data_,
                         std::make_unique<TriggerContext>(),
                         execute_callback.Get());
}

TEST_F(ScriptTrackerTest, NoRunnableScriptsEvenWithDOMChanges) {
  auto* script = AddScript();
  InitScriptProto(script, "path", "", "direct_action_name");
  script->mutable_presentation()->mutable_precondition()->add_path_pattern(
      "doesnotmatch");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(1, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, NoRunnableScriptsWaitingForDOMChanges) {
  InitScriptProto(AddScript(), "runnable path", "does_not_exist",
                  "direct_action_name");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

}  // namespace autofill_assistant
