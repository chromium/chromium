// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_script_executor.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

// Any isolated world id that the test doesn't otherwise use.
constexpr int32_t kWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;

// A minimal script that returns an empty manager.
constexpr char kValidScript[] = R"(
  function createCustomFormControlManager() {
    return {};
  }
)";

class CustomFormControlScriptExecutorTest : public content::RenderViewTest {
 protected:
  void SetUp() override {
    content::RenderViewTest::SetUp();
    LoadHTML("<div>Hello</div>");
    executor_ = std::make_unique<CustomFormControlScriptExecutor>(
        *GetMainRenderFrame(), kWorldId);
  }

  void TearDown() override {
    executor_.reset();
    content::RenderViewTest::TearDown();
  }

  // Returns whether `expression` is undefined in the main world.
  bool IsUndefinedInMainWorld(const std::string& expression) {
    int result = 0;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::UTF8ToUTF16("typeof " + expression + " === 'undefined' ? 1 : 0"),
        &result));
    return result == 1;
  }

  std::unique_ptr<CustomFormControlScriptExecutor> executor_;
};

// Tests that loading a valid script keeps its manager.
TEST_F(CustomFormControlScriptExecutorTest, LoadsManager) {
  EXPECT_FALSE(executor_->has_manager());
  EXPECT_TRUE(executor_->LoadScript(kValidScript));
  EXPECT_TRUE(executor_->has_manager());
}

// Tests that scripts ending with a line comment or without a semicolon load.
TEST_F(CustomFormControlScriptExecutorTest, LoadsScriptsWithUnusualEndings) {
  // Script ends with a comment.
  EXPECT_TRUE(executor_->LoadScript(
      "function createCustomFormControlManager() { return {}; } // end"));
  // Script ends without a ; at the end.
  EXPECT_TRUE(executor_->LoadScript(
      "const m = {}\n"
      "function createCustomFormControlManager() { return m }"));
}

// Tests that scripts can declare the entry point with `const` or `let`.
TEST_F(CustomFormControlScriptExecutorTest,
       LoadsEntryPointDeclaredWithLetOrConst) {
  for (const char* script : {
           "const createCustomFormControlManager = function() { return {}; };",
           "let createCustomFormControlManager = () => ({});",
       }) {
    SCOPED_TRACE(script);
    EXPECT_TRUE(executor_->LoadScript(script));
    EXPECT_TRUE(executor_->has_manager());
  }
}

// Tests that a failed load drops the manager of an earlier script.
TEST_F(CustomFormControlScriptExecutorTest, FailedLoadDropsPreviousManager) {
  ASSERT_TRUE(executor_->LoadScript(kValidScript));
  EXPECT_FALSE(executor_->LoadScript("var x = 1;"));
  EXPECT_FALSE(executor_->has_manager());
}

// Tests that loading fails, and no manager is kept, if the script doesn't
// declare the entry point as a function, throws, or the entry point's result
// isn't an object or is a function, array or promise.
TEST_F(CustomFormControlScriptExecutorTest, FailsWithoutValidManager) {
  for (const char* script : {
           "var x = 1;",
           "throw new Error('error');",
           "const createCustomFormControlManager = {};",
           "function createCustomFormControlManager() { return 42; }",
           "function createCustomFormControlManager() { return null; }",
           "function createCustomFormControlManager() {}",
           "function createCustomFormControlManager() { return []; }",
           "function createCustomFormControlManager() { return () => {}; }",
           "async function createCustomFormControlManager() { return {}; }",
           "function createCustomFormControlManager() { throw 1; }",
       }) {
    SCOPED_TRACE(script);
    EXPECT_FALSE(executor_->LoadScript(script));
    EXPECT_FALSE(executor_->has_manager());
  }
}

// Tests that the script's declarations and globals aren't visible to the page.
TEST_F(CustomFormControlScriptExecutorTest, ScriptDoesNotLeakIntoMainWorld) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    window.leaked = 1;
    function createCustomFormControlManager() { return {}; }
  )"));
  EXPECT_TRUE(IsUndefinedInMainWorld("createCustomFormControlManager"));
  EXPECT_TRUE(IsUndefinedInMainWorld("window.leaked"));
}

// Tests that a script that doesn't declare the entry point doesn't pick up a
// global left by an earlier script.
TEST_F(CustomFormControlScriptExecutorTest, IgnoresGlobalsOfEarlierScripts) {
  // Leave a global with the entry point's name in the isolated world.
  ASSERT_TRUE(executor_->LoadScript(R"(
    window.createCustomFormControlManager = () => ({});
    function createCustomFormControlManager() { return {}; }
  )"));
  // The global is still there for later scripts, so the next check isn't
  // passing just because the global is gone.
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return typeof window.createCustomFormControlManager === 'function'
          ? {} : null;
    }
  )"));
  // A script without the entry point fails instead of calling the global, and
  // the previous manager is dropped.
  EXPECT_FALSE(executor_->LoadScript("var x = 1;"));
  EXPECT_FALSE(executor_->has_manager());
}

// Tests that the manager is dropped when a new document is committed.
TEST_F(CustomFormControlScriptExecutorTest, DropsManagerOnNavigation) {
  ASSERT_TRUE(executor_->LoadScript(kValidScript));
  LoadHTML("<div>Another document</div>");
  EXPECT_FALSE(executor_->has_manager());
}

// Tests that the manager is kept across same-document navigations, which keep
// the document and its script contexts.
TEST_F(CustomFormControlScriptExecutorTest,
       KeepsManagerOnSameDocumentNavigation) {
  ASSERT_TRUE(executor_->LoadScript(kValidScript));
  ExecuteJavaScriptForTests("history.pushState({}, '', '#other');");
  EXPECT_TRUE(executor_->has_manager());
}

// Tests that top-level declarations of a script don't leak into scripts loaded
// later for the same document.
TEST_F(CustomFormControlScriptExecutorTest,
       DeclarationsDoNotLeakIntoLaterScripts) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    const x = 1;
    function createCustomFormControlManager() { return {}; }
  )"));
  // Redeclaring `x` would throw if it were a global of the isolated world.
  EXPECT_TRUE(executor_->LoadScript(R"(
    const x = 2;
    function createCustomFormControlManager() { return {}; }
  )"));
  // The earlier entry point is gone.
  EXPECT_FALSE(executor_->LoadScript("var y = 1;"));
}

// Stands in for the executor's owner (e.g. `AutofillAgent`), which destroys the
// executor in its own `OnDestruct()`. This lets a test destroy the executor
// while `LoadScript()` is still running the script that detaches the frame.
class DestroyExecutorOnFrameDestruction : public content::RenderFrameObserver {
 public:
  DestroyExecutorOnFrameDestruction(
      content::RenderFrame* render_frame,
      std::unique_ptr<CustomFormControlScriptExecutor>& executor)
      : content::RenderFrameObserver(render_frame), executor_(executor) {}

 private:
  void OnDestruct() override { executor_->reset(); }

  const raw_ref<std::unique_ptr<CustomFormControlScriptExecutor>> executor_;
};

// Tests that loading fails, without using the executor after it is destroyed,
// if the script detaches the frame and the owner destroys the executor.
TEST_F(CustomFormControlScriptExecutorTest,
       LoadFailsIfExecutorIsDestroyedDuringLoad) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  content::RenderFrame* child = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FirstChild()->ToWebLocalFrame());
  auto child_executor =
      std::make_unique<CustomFormControlScriptExecutor>(*child, kWorldId);
  DestroyExecutorOnFrameDestruction owner(child, child_executor);
  EXPECT_FALSE(child_executor->LoadScript(R"(
    frameElement.remove();
    function createCustomFormControlManager() { return {}; }
  )"));
  EXPECT_FALSE(child_executor);
}

// Tests that the manager of a subframe is dropped when the subframe is
// detached, which doesn't commit a new document.
TEST_F(CustomFormControlScriptExecutorTest, DropsManagerOnFrameDetach) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child = GetMainFrame()->FirstChild()->ToWebLocalFrame();
  CustomFormControlScriptExecutor child_executor(
      *content::RenderFrame::FromWebFrame(child), kWorldId);
  ASSERT_TRUE(child_executor.LoadScript(kValidScript));

  ExecuteJavaScriptForTests("document.querySelector('iframe').remove();");
  EXPECT_FALSE(child_executor.has_manager());
}

// Tests that `document.open()` in a script, which keeps the document's script
// contexts, doesn't prevent the script from loading.
TEST_F(CustomFormControlScriptExecutorTest, LoadsScriptThatOpensDocument) {
  EXPECT_TRUE(executor_->LoadScript(R"(
    document.open();
    document.close();
    function createCustomFormControlManager() { return {}; }
  )"));
  EXPECT_TRUE(executor_->has_manager());
}

}  // namespace
}  // namespace autofill
