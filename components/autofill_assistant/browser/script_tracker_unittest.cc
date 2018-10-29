// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class ScriptTrackerTest : public testing::Test,
                          public ScriptTracker::Listener,
                          public ScriptExecutorDelegate {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_,
            OnElementCheck(kExistenceCheck, ElementsAre("exists"), _))
        .WillByDefault(RunOnceCallback<2>(true));
    ON_CALL(mock_web_controller_,
            OnElementCheck(kExistenceCheck, ElementsAre("does_not_exist"), _))
        .WillByDefault(RunOnceCallback<2>(false));
    ON_CALL(mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));

    // Scripts run, but have no actions.
    ON_CALL(mock_service_, OnGetActions(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, ""));
  }

 protected:
  ScriptTrackerTest() : runnable_scripts_changed_(0), tracker_(this, this) {}

  // Overrides ScriptTrackerDelegate
  Service* GetService() override { return &mock_service_; }

  UiController* GetUiController() override { return &mock_ui_controller_; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return &client_memory_; }

  const std::map<std::string, std::string>& GetParameters() override {
    return parameters_;
  }

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }

  content::WebContents* GetWebContents() override { return nullptr; }

  // Overrides ScriptTracker::Listener
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override {
    runnable_scripts_changed_++;
    runnable_scripts_ = runnable_scripts;
  }

  void SetAndCheckScripts(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);
    std::vector<std::unique_ptr<Script>> scripts;
    ProtocolUtils::ParseScripts(response_str, &scripts);
    tracker_.SetScripts(std::move(scripts));
    tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));
  }

  static SupportedScriptProto* AddScript(SupportsScriptResponseProto* response,
                                         const std::string& name,
                                         const std::string& path,
                                         const std::string& selector) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(path);
    script->mutable_presentation()->set_name(name);
    script->mutable_presentation()
        ->mutable_precondition()
        ->add_elements_exist()
        ->add_selectors(selector);
    ScriptStatusMatchProto dont_run_twice_precondition;
    dont_run_twice_precondition.set_script(path);
    dont_run_twice_precondition.set_comparator(ScriptStatusMatchProto::EQUAL);
    dont_run_twice_precondition.set_status(SCRIPT_STATUS_NOT_RUN);
    *script->mutable_presentation()
         ->mutable_precondition()
         ->add_script_status_match() = dont_run_twice_precondition;
    return script;
  }

  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path) {
    return AddScript(response, name_and_path, name_and_path, "exists");
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

  GURL url_;
  NiceMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  NiceMock<MockUiController> mock_ui_controller_;
  ClientMemory client_memory_;
  std::map<std::string, std::string> parameters_;

  // Number of times OnRunnableScriptsChanged was called.
  int runnable_scripts_changed_;
  std::vector<ScriptHandle> runnable_scripts_;
  ScriptTracker tracker_;
};

TEST_F(ScriptTrackerTest, NoScripts) {
  tracker_.SetScripts({});
  EXPECT_EQ(0, runnable_scripts_changed_);
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, SomeRunnableScripts) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "not runnable name", "not runnable path",
            "does_not_exist");
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);

  EXPECT_EQ(1, runnable_scripts_changed_);
  ASSERT_THAT(runnable_scripts(), SizeIs(1));
  EXPECT_EQ("runnable name", runnable_scripts()[0].name);
  EXPECT_EQ("runnable path", runnable_scripts()[0].path);
}

TEST_F(ScriptTrackerTest, OrderScriptsByPriority) {
  SupportsScriptResponseProto scripts;

  SupportedScriptProto* a = scripts.add_scripts();
  a->set_path("a");
  a->mutable_presentation()->set_name("a");
  a->mutable_presentation()->set_priority(2);

  SupportedScriptProto* b = scripts.add_scripts();
  b->set_path("b");
  b->mutable_presentation()->set_name("b");
  b->mutable_presentation()->set_priority(3);

  SupportedScriptProto* c = scripts.add_scripts();
  c->set_path("c");
  c->mutable_presentation()->set_name("c");
  c->mutable_presentation()->set_priority(1);
  SetAndCheckScripts(scripts);

  ASSERT_THAT(runnable_script_paths(), ElementsAre("c", "a", "b"));
}

TEST_F(ScriptTrackerTest, NewScriptChangesNothing) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, NewScriptClearsRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  SetAndCheckScripts(SupportsScriptResponseProto::default_instance());
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), IsEmpty());
}

TEST_F(ScriptTrackerTest, NewScriptAddsRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  AddScript(&scripts, "new runnable name", "new runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(2));
}

TEST_F(ScriptTrackerTest, NewScriptChangesRunnable) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_scripts(), SizeIs(1));

  scripts.clear_scripts();
  AddScript(&scripts, "new runnable name", "new runnable path", "exists");
  SetAndCheckScripts(scripts);
  EXPECT_EQ(2, runnable_scripts_changed_);
}

TEST_F(ScriptTrackerTest, CheckScriptsAgainAfterScriptEnd) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "script 1", "script1", "exists");
  AddScript(&scripts, "script 2", "script2", "exists");
  SetAndCheckScripts(scripts);

  // Both scripts are runnable
  EXPECT_EQ(1, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(),
              UnorderedElementsAre("script1", "script2"));

  // run 'script 1'
  base::MockCallback<ScriptExecutor::RunScriptCallback> execute_callback;
  EXPECT_CALL(execute_callback,
              Run(Field(&ScriptExecutor::Result::success, true)));

  tracker_.ExecuteScript("script1", execute_callback.Get());
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // The 2nd time the scripts are checked, automatically after the script runs,
  // 'script1' isn't runnable anymore, because it's already been run.
  EXPECT_EQ(2, runnable_scripts_changed_);
  EXPECT_THAT(runnable_script_paths(), ElementsAre("script2"));
}

TEST_F(ScriptTrackerTest, CheckScriptsAfterDOMChange) {
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(kExistenceCheck, ElementsAre("maybe_exists"), _))
      .WillOnce(RunOnceCallback<2>(false));

  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "script name", "script path", "maybe_exists");
  SetAndCheckScripts(scripts);

  // No scripts are runnable.
  EXPECT_THAT(runnable_scripts(), IsEmpty());

  // DOM has changed; OnElementExists now returns true.
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(kExistenceCheck, ElementsAre("maybe_exists"), _))
      .WillOnce(RunOnceCallback<2>(true));
  tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));

  // The script can now run
  ASSERT_THAT(runnable_script_paths(), ElementsAre("script path"));
}

TEST_F(ScriptTrackerTest, DuplicateCheckCalls) {
  SupportsScriptResponseProto scripts;
  AddScript(&scripts, "runnable name", "runnable path", "exists");

  base::OnceCallback<void(bool)> captured_callback;
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(kExistenceCheck, ElementsAre("exists"), _))
      .WillOnce(CaptureOnceCallback<2>(&captured_callback))
      .WillOnce(RunOnceCallback<2>(false));
  SetAndCheckScripts(scripts);

  // At this point, since the callback hasn't been run, there's still a check in
  // progress. The three calls to CheckScripts will trigger one call to
  // CheckScript right after first_call has run.
  for (int i = 0; i < 3; i++) {
    tracker_.CheckScripts(base::TimeDelta::FromSeconds(0));
  }

  EXPECT_THAT(runnable_scripts(), IsEmpty());
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback).Run(true);

  // The second check is run right away, after the first check, say that the
  // element doesn't exist anymore, and we end up again with an empty
  // runnable_scripts.
  EXPECT_THAT(runnable_scripts(), IsEmpty());
  EXPECT_EQ(2, runnable_scripts_changed_);
}

}  // namespace autofill_assistant
