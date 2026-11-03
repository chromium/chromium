// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_script_executor.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace autofill {

// Gives tests access to `CustomFormControlScriptExecutor::CallFunction()`.
class CustomFormControlScriptExecutorTestApi {
 public:
  using Argument = CustomFormControlScriptExecutor::Argument;
  using ResultCallback = CustomFormControlScriptExecutor::ResultCallback;

  explicit CustomFormControlScriptExecutorTestApi(
      CustomFormControlScriptExecutor* executor)
      : executor_(CHECK_DEREF(executor)) {}

  void CallFunction(std::string_view function_name,
                    std::vector<Argument> arguments,
                    ResultCallback callback) {
    executor_->CallFunction(function_name, std::move(arguments),
                            std::move(callback));
  }

 private:
  const raw_ref<CustomFormControlScriptExecutor> executor_;
};

namespace {

// Any isolated world id that the test doesn't otherwise use.
constexpr int32_t kWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;

using Argument = CustomFormControlScriptExecutorTestApi::Argument;

// A minimal script that returns an empty manager.
constexpr char kValidScript[] = R"(
  function createCustomFormControlManager() {
    return {};
  }
)";

// Copies `string` into a `std::string` as UTF-8.
std::string ToUtf8(v8::Isolate* isolate, v8::Local<v8::String> string) {
  std::string utf8(string->Utf8Length(isolate), '\0');
  string->WriteUtf8(isolate, utf8.data(), utf8.size());
  return utf8;
}

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

  // Evaluates `expression` in the executor's isolated world of the main frame
  // and returns the result if it is a string.
  std::optional<std::string> EvaluateInIsolatedWorld(
      const std::string& expression) {
    v8::Isolate* isolate = GetMainFrame()->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result =
        GetMainFrame()->ExecuteScriptInIsolatedWorldAndReturnValue(
            kWorldId,
            blink::WebScriptSource(blink::WebString::FromUtf8(expression)),
            blink::BackForwardCacheAware::kAllow);
    // Converting other values would run script (e.g. `toString()`).
    if (result.IsEmpty() || !result->IsString()) {
      return std::nullopt;
    }
    return ToUtf8(isolate, result.As<v8::String>());
  }

  // Calls `function_name` and returns the result converted to a string, or
  // `std::nullopt` if the call or the conversion failed.
  std::optional<std::string> Call(std::string_view function_name,
                                  std::vector<Argument> arguments = {}) {
    std::optional<std::string> result;
    bool called = false;
    CustomFormControlScriptExecutorTestApi(executor_.get())
        .CallFunction(
            function_name, std::move(arguments),
            base::BindLambdaForTesting([&](v8::Isolate* isolate,
                                           v8::Local<v8::Context> context,
                                           v8::MaybeLocal<v8::Value> value) {
              called = true;
              // Converting the value may run script (e.g. `toString()`), which
              // may throw.
              v8::TryCatch try_catch(isolate);
              v8::Local<v8::Value> local;
              v8::Local<v8::String> string;
              if (value.ToLocal(&local) &&
                  local->ToString(context).ToLocal(&string)) {
                result = ToUtf8(isolate, string);
              }
            }));
    EXPECT_TRUE(called);
    return result;
  }

  blink::WebNode GetTarget() {
    return GetMainFrame()->GetDocument().GetElementById("target");
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

// Stands in for an owner that destroys the executor synchronously in its own
// `OnDestruct()` (unlike `AutofillAgent`, which is deleted asynchronously).
// This lets a test destroy the executor while `LoadScript()` is still running
// the script that detaches the frame.
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

// Tests that a manager whose `toString()` and `valueOf()` throw loads, since
// loading doesn't convert the manager.
TEST_F(CustomFormControlScriptExecutorTest,
       LoadsManagerThatThrowsOnConversion) {
  EXPECT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        toString() { throw new Error('error'); },
        valueOf() { throw new Error('error'); },
      };
    }
  )"));
  EXPECT_TRUE(executor_->has_manager());
}

// Tests that the isolated world's Content Security Policy blocks `eval()` and
// `new Function()` in scripts, without affecting the page.
TEST_F(CustomFormControlScriptExecutorTest, IsolatedWorldBlocksEval) {
  ASSERT_TRUE(executor_->LoadScript(kValidScript));
  EXPECT_EQ(EvaluateInIsolatedWorld(
                "try { eval('1'); 'allowed' } catch (e) { 'blocked' }"),
            "blocked");
  EXPECT_EQ(EvaluateInIsolatedWorld(
                "try { new Function('1'); 'allowed' } catch (e) { 'blocked' }"),
            "blocked");
  // The page's own scripts may still use `eval()`.
  int result = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"eval('1 + 1')", &result));
  EXPECT_EQ(result, 2);
}

// Tests that scripts can still access the DOM of their frame under the
// isolated world's Content Security Policy.
TEST_F(CustomFormControlScriptExecutorTest, IsolatedWorldCanAccessDom) {
  LoadHTML("<div id=target>Hello</div>");
  ASSERT_TRUE(executor_->LoadScript(kValidScript));
  EXPECT_EQ(
      EvaluateInIsolatedWorld("document.getElementById('target').textContent"),
      "Hello");
}

// Tests that calls pass DOM nodes and strings to the manager's methods, with
// the manager as `this`, and return their result.
TEST_F(CustomFormControlScriptExecutorTest, CallsFunctionWithArguments) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        tag: 'manager',
        describe(node, s, u) {
          return [node.id, s, u === undefined, this.tag].join(',');
        },
      };
    }
  )"));
  EXPECT_EQ(Call("describe", {GetTarget(), u"text", std::nullopt}),
            "target,text,true,manager");
}

// Tests that calls run in the isolated world, so that properties they set on
// DOM nodes aren't visible to the page.
TEST_F(CustomFormControlScriptExecutorTest, CallsUseIsolatedWorld) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return { mark(node) { node.marked = 1; return node.marked; } };
    }
  )"));
  EXPECT_EQ(Call("mark", {GetTarget()}), "1");
  EXPECT_TRUE(
      IsUndefinedInMainWorld("document.getElementById('target').marked"));
}

// Tests that loading another script replaces the manager, so that calls go to
// the new one.
TEST_F(CustomFormControlScriptExecutorTest, ReplacesManager) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return { name: () => 'first' };
    }
  )"));
  EXPECT_EQ(Call("name"), "first");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return { name: () => 'second' };
    }
  )"));
  EXPECT_EQ(Call("name"), "second");
}

// Tests that calls fail if there is no manager or no such method, if an
// argument is a null node, or if the method throws or returns a promise, and
// that later calls still work.
TEST_F(CustomFormControlScriptExecutorTest, CallFails) {
  EXPECT_EQ(Call("anything"), std::nullopt);  // No manager.
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        notAFunction: 1,
        throws() { throw new Error('error'); },
        async isAsync() { return 1; },
        works() { return 'ok'; },
      };
    }
  )"));
  EXPECT_EQ(Call("missing"), std::nullopt);
  EXPECT_EQ(Call("notAFunction"), std::nullopt);
  EXPECT_EQ(Call("throws"), std::nullopt);
  EXPECT_EQ(Call("isAsync"), std::nullopt);
  EXPECT_EQ(Call("works", {blink::WebNode()}), std::nullopt);  // Null node.
  EXPECT_EQ(Call("works"), "ok");
}

// Tests that a result whose conversion throws (here in `Call()`, which runs
// `toString()`) doesn't affect later calls.
TEST_F(CustomFormControlScriptExecutorTest, ResultThatThrowsOnConversion) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        throwsOnConversion() {
          return { toString() { throw new Error('error'); } };
        },
        works() { return 'ok'; },
      };
    }
  )"));
  EXPECT_EQ(Call("throwsOnConversion"), std::nullopt);
  EXPECT_EQ(Call("works"), "ok");
}

// Tests that the microtasks a call queues only run at the next microtask
// checkpoint, after the call.
TEST_F(CustomFormControlScriptExecutorTest, MicrotasksRunAfterCall) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        queuesMicrotask() {
          queueMicrotask(() => { this.ran = true; });
          return 'ok';
        },
        ran: false,
        microtaskRan() { return String(this.ran); },
      };
    }
  )"));
  EXPECT_EQ(Call("queuesMicrotask"), "ok");
  EXPECT_EQ(Call("microtaskRan"), "false");
  // Running a script in the page performs a microtask checkpoint afterwards,
  // which also runs the isolated world's microtasks.
  ExecuteJavaScriptForTests("0;");
  EXPECT_EQ(Call("microtaskRan"), "true");
}

// Tests that a call fails, without crashing, if the method detaches the frame,
// which releases the isolated world's script context during the call.
TEST_F(CustomFormControlScriptExecutorTest, CallFailsIfFrameIsDetached) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child = GetMainFrame()->FirstChild()->ToWebLocalFrame();
  CustomFormControlScriptExecutor child_executor(
      *content::RenderFrame::FromWebFrame(child), kWorldId);
  ASSERT_TRUE(child_executor.LoadScript(R"(
    function createCustomFormControlManager() {
      return { detach() { frameElement.remove(); return 'detached'; } };
    }
  )"));
  bool called = false;
  CustomFormControlScriptExecutorTestApi(&child_executor)
      .CallFunction(
          "detach", {},
          base::BindLambdaForTesting([&](v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::MaybeLocal<v8::Value> value) {
            called = true;
            EXPECT_TRUE(context.IsEmpty());
            EXPECT_TRUE(value.IsEmpty());
          }));
  EXPECT_TRUE(called);
  EXPECT_FALSE(child_executor.has_manager());
}

// Tests that a call that fails while the frame is alive still passes the
// isolate.
TEST_F(CustomFormControlScriptExecutorTest, FailedCallPassesIsolate) {
  bool called = false;
  CustomFormControlScriptExecutorTestApi(executor_.get())
      .CallFunction(
          "anything", {},
          base::BindLambdaForTesting([&](v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::MaybeLocal<v8::Value> value) {
            called = true;
            EXPECT_TRUE(isolate);
            EXPECT_TRUE(context.IsEmpty());
            EXPECT_TRUE(value.IsEmpty());
          }));
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace autofill
