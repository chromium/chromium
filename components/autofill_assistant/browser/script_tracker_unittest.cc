// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class ScriptTrackerTest : public testing::Test, public ScriptTracker::Listener {
 public:
  void SetUp() override {
    delegate_.SetCurrentURL(GURL("http://www.example.com/"));

    ON_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"exists"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_,
            OnElementCheck(Eq(Selector({"does_not_exist"})), _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));

    // Scripts run, but have no actions.
    ON_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(true, ""));
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

  void SetAndCheckScripts() {
    std::vector<std::unique_ptr<Script>> scripts;
    for (const auto& script_proto : scripts_proto_) {
      ProtocolUtils::AddScript(*script_proto, &scripts);
    }
    tracker_.SetScripts(std::move(scripts));
    tracker_.CheckScripts();
  }

  SupportedScriptProto* AddScript() {
    scripts_proto_.emplace_back(new SupportedScriptProto);
    return scripts_proto_.back().get();
  }

  SupportedScriptProto* AddScript(const std::string& name,
                                  const std::string& path,
                                  const std::string& selector) {
    SupportedScriptProto* script = AddScript();
    script->set_path(path);
    script->mutable_presentation()->mutable_chip()->set_text(name);
    if (!selector.empty()) {
      script->mutable_presentation()
          ->mutable_precondition()
          ->add_elements_exist()
          ->add_selectors(selector);
    }
    ScriptStatusMatchProto dont_run_twice_precondition;
    dont_run_twice_precondition.set_script(path);
    dont_run_twice_precondition.set_comparator(ScriptStatusMatchProto::EQUAL);
    dont_run_twice_precondition.set_status(SCRIPT_STATUS_NOT_RUN);
    *script->mutable_presentation()
         ->mutable_precondition()
         ->add_script_status_match() = dont_run_twice_precondition;
    return script;
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
};

TEST_F(ScriptTrackerTest, NoScripts) {
  tracker_.SetScripts({});
  tracker_.CheckScripts();
  EXPECT_THAT(runnable_scripts(), IsEmpty());
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, SomeRunnableScripts) {
  AddScript("not runnable name", "not runnable path", "does_not_exist");
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, DoNotCheckInterruptWithNoName) {
  // The interrupt's preconditions would all be met, but it won't be reported
  // since it doesn't have a name.
  auto* no_name = AddScript("", "path1", "exists");
  no_name->mutable_presentation()->set_interrupt(true);

  // The interrupt's preconditions are met and it will be reported as a normal
  // script.
  auto* with_name = AddScript("with name", "path2", "exists");
  with_name->mutable_presentation()->set_interrupt(true);

  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("with name", runnable_scripts()[0].chip.text);
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
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, NewScriptClearsRunnable) {
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, NewScriptAddsRunnable) {
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  AddScript("new runnable name", "new runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(2));
}

TEST_F(ScriptTrackerTest, NewScriptChangesRunnable) {
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts_proto_.clear();
  AddScript("new runnable name", "new runnable path", "exists");
  SetAndCheckScripts();
  EXPECT_EQ(2, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, CheckScriptsAgainAfterScriptEnd) {
  AddScript("script 1", "script1", "exists");
  AddScript("script 2", "script2", "exists");
  SetAndCheckScripts();

  // Both scripts are runnable
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(),
              UnorderedElementsAre("script1", "script2"));

  // run 'script 1'
  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));

  tracker_.ExecuteScript("script1", TriggerContext::CreateEmpty(),
                         execute_callback.Get());
  tracker_.CheckScripts();

  // The 2nd time the scripts are checked, automatically after the script runs,
  // 'script1' isn't runnable anymore, because it's already been run.
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(), ElementsAre("script2"));
}

TEST_F(ScriptTrackerTest, CheckScriptsAfterDOMChange) {
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"maybe_exists"})), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus()));

  AddScript("script name", "script path", "maybe_exists");
  SetAndCheckScripts();

  // No scripts are runnable.
  EXPECT_THAT(runnable_scripts(), IsEmpty());

  // DOM has changed; OnElementExists now returns truthy.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"maybe_exists"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  tracker_.CheckScripts();

  // The script can now run
  ASSERT_THAT(runnable_script_paths(), ElementsAre("script path"));
}

TEST_F(ScriptTrackerTest, UpdateScriptList) {
  // 1. Initialize runnable scripts with a single valid script.
  AddScript("runnable name", "runnable path", "exists");
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the action and trigger a script list update.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript("update name", "update path", "exists");
  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript("update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", TriggerContext::CreateEmpty(),
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
  auto* script = AddScript("runnable name", "runnable path", "exists");
  script->mutable_presentation()->set_interrupt(true);
  SetAndCheckScripts();

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].chip.text);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);

  // 2. Run the interrupt action and trigger a script list update from an
  // interrupt.
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");

  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript("update name", "update path", "exists");
  *actions_response.mutable_update_script_list()->add_scripts() =
      *AddScript("update name 2", "update path 2", "exists");

  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq("runnable name"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(true, ""));

  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));
  tracker_.ExecuteScript("runnable name", TriggerContext::CreateEmpty(),
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

TEST_F(ScriptTrackerTest, NoRunnableScriptsEvenWithDOMChanges) {
  auto* script = AddScript("name", "path", "");
  script->mutable_presentation()->mutable_precondition()->add_path_pattern(
      "doesnotmatch");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(1, no_runnable_scripts_anymore_);
}

TEST_F(ScriptTrackerTest, NoRunnableScriptsWaitingForDOMChanges) {
  AddScript("runnable name", "runnable path", "does_not_exist");
  SetAndCheckScripts();

  EXPECT_THAT(runnable_scripts(), SizeIs(0));
  EXPECT_EQ(0, no_runnable_scripts_anymore_);
}

}  // namespace autofill_assistant
