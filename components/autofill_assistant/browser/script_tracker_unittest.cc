// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
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
using ::testing::ReturnRef;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

class ScriptTrackerTest : public testing::Test, public ScriptTracker::Listener {
 public:
  void SetUp() override {
    delegate_.SetCurrentURL(GURL("http://www.example.com/"));

    ON_CALL(mock_web_controller_, OnFindElement(Selector({"exists"}), _))
        .WillByDefault(WithArgs<1>([](auto&& callback) {
          std::move(callback).Run(OkClientStatus(),
                                  std::make_unique<ElementFinder::Result>());
        }));
    ON_CALL(mock_web_controller_,
            OnFindElement(Selector({"does_not_exist"}), _))
        .WillByDefault(RunOnceCallback<1>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

    // Scripts run, but have no actions.
    ON_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(net::HTTP_OK, ""));
  }

 protected:
  ScriptTrackerTest()
      : no_runnable_scripts_anymore_(0),
        runnable_scripts_changed_(0),
        tracker_(&delegate_, /* listener=*/this) {
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
                       const std::string& name,
                       const std::string& path,
                       const std::string& selector) {
    script->set_path(path);
    script->mutable_presentation()->mutable_chip()->set_text(name);
    if (!selector.empty()) {
      *script->mutable_presentation()
           ->mutable_precondition()
           ->mutable_element_condition()
           ->mutable_match() = ToSelectorProto(selector);
    }
    ScriptStatusMatchProto dont_run_twice_precondition;
    dont_run_twice_precondition.set_script(path);
    dont_run_twice_precondition.set_comparator(ScriptStatusMatchProto::EQUAL);
    dont_run_twice_precondition.set_status(SCRIPT_STATUS_NOT_RUN);
    *script->mutable_presentation()
         ->mutable_precondition()
         ->add_script_status_match() = dont_run_twice_precondition;
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
  InitScriptProto(AddScript(), "not runnable name", "not runnable path",
                  "does_not_exist");
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, DoNotReportInterruptsAsRunnable) {
  // The interrupt's preconditions would all be met, but it won't be reported as
  // runnable since it's an interrupt.
  auto* interrupt = AddScript();
  InitScriptProto(interrupt, "name", "path1", "exists");
  interrupt->mutable_presentation()->set_interrupt(true);

  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, OrderScriptsByPriority) {
  SupportedScriptProto* a = AddScript();
  a->set_path("a");
  a->mutable_presentation()->mutable_chip()->set_text("a");
  a->mutable_presentation()->set_priority(2);

  SupportedScriptProto* b = AddScript();
  b->set_path("b");
  b->mutable_presentation()->mutable_chip()->set_text("b");
  b->mutable_presentation()->set_priority(3);

  SupportedScriptProto* c = AddScript();
  c->set_path("c");
  c->mutable_presentation()->mutable_chip()->set_text("c");
  c->mutable_presentation()->set_priority(1);

  SetAndCheckScripts();

  ASSERT_THAT(runnable_script_paths(), ElementsAre("c", "a", "b"));
}

TEST_F(ScriptTrackerTest, NewScriptChangesNothing) {
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, NewScriptClearsRunnable) {
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, NewScriptAddsRunnable) {
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  InitScriptProto(AddScript(), "new runnable name", "new runnable path",
                  "exists");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(2));
}

TEST_F(ScriptTrackerTest, NewScriptChangesRunnable) {
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  InitScriptProto(AddScript(), "new runnable name", "new runnable path",
                  "exists");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, CheckScriptsAgainAfterScriptEnd) {
  InitScriptProto(AddScript(), "script 1", "script1", "exists");
  InitScriptProto(AddScript(), "script 2", "script2", "exists");
  SetAndCheckScripts();

  // Both scripts are runnable
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(),
              UnorderedElementsAre("script1", "script2"));

  // run 'script 1'
  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));

  tracker_.ExecuteScript("script1", &user_data_,
                         std::make_unique<TriggerContext>(),
                         execute_callback.Get());
  tracker_.CheckScripts();

  // The 2nd time the scripts are checked, automatically after the script runs,
  // 'script1' isn't runnable anymore, because it's already been run.
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(), ElementsAre("script2"));
}

TEST_F(ScriptTrackerTest, CheckScriptsAfterDOMChange) {
  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector({"maybe_exists"}), _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  InitScriptProto(AddScript(), "script name", "script path", "maybe_exists");
  SetAndCheckScripts();

  // No scripts are runnable.
  EXPECT_THAT(runnable_scripts(), IsEmpty());

  // DOM has changed; OnElementExists now returns truthy.
  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector({"maybe_exists"}), _))
      .WillOnce(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
  tracker_.CheckScripts();

  // The script can now run
  ASSERT_THAT(runnable_script_paths(), ElementsAre("script path"));
}

TEST_F(ScriptTrackerTest, UpdateScriptList) {
  // 1. Initialize runnable scripts with a single valid script.
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the action and trigger a script list update.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update name", "update path", "exists");
  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, ""));

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
  EXPECT_EQ("update name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update name 2", runnable_scripts()[1].chip.text);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

TEST_F(ScriptTrackerTest, UpdateScriptListFromInterrupt) {
  // 1. Initialize runnable scripts with a single valid interrupt script.
  InitScriptProto(AddScript(), "runnable name", "runnable path", "exists");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the interrupt action and trigger a script list update from an
  // interrupt.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update name", "update path", "exists");
  InitScriptProto(actions_response.mutable_update_script_list()->add_scripts(),
                  "update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, ""));

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
  EXPECT_EQ("update name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("update path", runnable_scripts()[0].path);
  EXPECT_EQ("update name 2", runnable_scripts()[1].chip.text);
  EXPECT_EQ("update path 2", runnable_scripts()[1].path);
}

TEST_F(ScriptTrackerTest, UpdateInterruptList) {
  // The first GetActions response from "main" updates the set of scripts, which
  // includes "interrupt". "interrupt" is then executed from the wait_for_dom.

  InitScriptProto(AddScript(), "main", "main", "exists");
  SetScripts();

  ActionsResponseProto actions_response;
  auto* wait_for_dom = actions_response.add_actions()->mutable_wait_for_dom();
  *wait_for_dom->mutable_wait_condition()->mutable_match() =
      ToSelectorProto("exists");
  wait_for_dom->set_allow_interrupt(true);

  SupportedScriptProto* interrupt_proto =
      actions_response.mutable_update_script_list()->add_scripts();
  InitScriptProto(interrupt_proto, "interrupt", "interrupt", "exists");
  interrupt_proto->mutable_presentation()->set_interrupt(true);

  EXPECT_CALL(mock_service_, OnGetActions("main", _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(net::HTTP_OK, ""));

  ActionsResponseProto actions_interrupt;
  actions_response.set_script_payload("from interrupt");
  actions_response.add_actions()->mutable_tell()->set_message("interrupt");

  EXPECT_CALL(mock_service_, OnGetActions("interrupt", _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(net::HTTP_OK, Serialize(actions_interrupt)));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("main", &user_data_,
                         std::make_unique<TriggerContext>(),
                         execute_callback.Get());
}

TEST_F(ScriptTrackerTest, NoRunnableScriptsEvenWithDOMChanges) {
  auto* script = AddScript();
  InitScriptProto(script, "name", "path", "");
  script->mutable_presentation()->mutable_precondition()->add_path_pattern(
      "doesnotmatch");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(1, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, NoRunnableScriptsWaitingForDOMChanges) {
  InitScriptProto(AddScript(), "runnable name", "runnable path",
                  "does_not_exist");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

}  // namespace autofill_assistant
