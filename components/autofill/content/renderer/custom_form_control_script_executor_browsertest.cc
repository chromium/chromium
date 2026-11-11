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
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "components/autofill/content/renderer/custom_form_control_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  using MayReturnPromise = CustomFormControlScriptExecutor::MayReturnPromise;
  using ResultCallback = CustomFormControlScriptExecutor::ResultCallback;

  explicit CustomFormControlScriptExecutorTestApi(
      CustomFormControlScriptExecutor* executor)
      : executor_(CHECK_DEREF(executor)) {}

  void CallFunction(MayReturnPromise may_return_promise,
                    std::string_view function_name,
                    std::vector<Argument> arguments,
                    ResultCallback callback) {
    executor_->CallFunction(may_return_promise, function_name,
                            std::move(arguments), std::move(callback));
  }

 private:
  const raw_ref<CustomFormControlScriptExecutor> executor_;
};

namespace {

// Any isolated world id that the test doesn't otherwise use.
constexpr int32_t kWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;

using Argument = CustomFormControlScriptExecutorTestApi::Argument;
using MayReturnPromise =
    CustomFormControlScriptExecutorTestApi::MayReturnPromise;

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
        GetMainRenderFrame(), kWorldId);
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

  struct CallResult {
    bool done = false;
    // Whether the callback got an isolate.
    bool had_isolate = false;
    // The result converted to a string, or `std::nullopt` if the call or the
    // conversion failed.
    std::optional<std::string> value;
  };

  // Starts a call. `result` is updated when the call finishes.
  void StartCall(MayReturnPromise may_return_promise,
                 std::string_view function_name,
                 CallResult& result,
                 std::vector<Argument> arguments = {}) {
    CustomFormControlScriptExecutorTestApi(executor_.get())
        .CallFunction(
            may_return_promise, function_name, std::move(arguments),
            base::BindLambdaForTesting(
                [&result](v8::Isolate* isolate, v8::Local<v8::Context> context,
                          v8::MaybeLocal<v8::Value> value) {
                  result.done = true;
                  result.had_isolate = isolate;
                  // The call failed if the frame is gone.
                  if (!isolate) {
                    return;
                  }
                  // Converting the value may run script (e.g. `toString()`),
                  // which may throw.
                  v8::TryCatch try_catch(isolate);
                  v8::Local<v8::Value> local;
                  v8::Local<v8::String> string;
                  if (value.ToLocal(&local) &&
                      local->ToString(context).ToLocal(&string)) {
                    result.value = ToUtf8(isolate, string);
                  }
                }));
  }

  // Makes a call that may return a promise and is expected to finish
  // synchronously. Returns the result converted to a string, or `std::nullopt`
  // if the call or the conversion failed.
  std::optional<std::string> Call(std::string_view function_name,
                                  std::vector<Argument> arguments = {}) {
    CallResult result;
    StartCall(MayReturnPromise(true), function_name, result,
              std::move(arguments));
    EXPECT_TRUE(result.done);
    return result.value;
  }

  blink::WebElement GetTarget() {
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

// Tests that loading fails, and no manager is kept, if the script detaches
// the frame.
TEST_F(CustomFormControlScriptExecutorTest, LoadFailsIfFrameIsDetached) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  content::RenderFrame* child = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FirstChild()->ToWebLocalFrame());
  CustomFormControlScriptExecutor child_executor(child, kWorldId);
  EXPECT_FALSE(child_executor.LoadScript(R"(
    frameElement.remove();
    function createCustomFormControlManager() { return {}; }
  )"));
  EXPECT_FALSE(child_executor.has_manager());
}

// Tests that the manager of a subframe is dropped when the subframe is
// detached, which doesn't commit a new document.
TEST_F(CustomFormControlScriptExecutorTest, DropsManagerOnFrameDetach) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child = GetMainFrame()->FirstChild()->ToWebLocalFrame();
  CustomFormControlScriptExecutor child_executor(
      content::RenderFrame::FromWebFrame(child), kWorldId);
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

constexpr char kClosePopoverScript[] = R"(
  function createCustomFormControlManager() {
    return {
      closePopover(node) {
        switch (node.dataset.mode) {
          case 'sync': node.dataset.closed = 'sync'; return 42;
          case 'throw': throw new Error('error');
          case 'reject': return Promise.reject(new Error('error'));
          default:
            return new Promise(r => setTimeout(() => {
              node.dataset.closed = 'async';
              r();
            }, 0));
        }
      },
    };
  }
)";

// Tests that `ClosePopover()` passes the element to `closePopover()` and
// succeeds once its promise is fulfilled, or right away if it returns
// something else.
TEST_F(CustomFormControlScriptExecutorTest, ClosesPopover) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(kClosePopoverScript));
  base::test::TestFuture<bool> later;
  executor_->ClosePopover(GetTarget(), later.GetCallback());
  EXPECT_FALSE(later.IsReady());
  EXPECT_TRUE(later.Get());
  EXPECT_EQ(GetTarget().GetAttribute("data-closed").Utf8(), "async");

  ExecuteJavaScriptForTests(
      "document.getElementById('target').dataset.mode = 'sync';");
  base::test::TestFuture<bool> now;
  executor_->ClosePopover(GetTarget(), now.GetCallback());
  ASSERT_TRUE(now.IsReady());
  EXPECT_TRUE(now.Get());
  EXPECT_EQ(GetTarget().GetAttribute("data-closed").Utf8(), "sync");
}

// Tests that `ClosePopover()` fails if there is no manager, or if
// `closePopover()` throws or its promise is rejected.
TEST_F(CustomFormControlScriptExecutorTest, ClosePopoverFails) {
  LoadHTML("<div id=target></div>");
  base::test::TestFuture<bool> no_manager;
  executor_->ClosePopover(GetTarget(), no_manager.GetCallback());
  EXPECT_FALSE(no_manager.Get());

  ASSERT_TRUE(executor_->LoadScript(kClosePopoverScript));
  for (const char* mode : {"throw", "reject"}) {
    SCOPED_TRACE(mode);
    ExecuteJavaScriptForTests(
        std::string("document.getElementById('target').dataset.mode = '") +
        mode + "';");
    base::test::TestFuture<bool> future;
    executor_->ClosePopover(GetTarget(), future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

using OptionsFuture =
    base::test::TestFuture<std::optional<CustomFormControlOptions>>;

// Tests that `ExtractOptions()` passes the element to `extractOptions()` and
// returns the options once its promise is fulfilled.
TEST_F(CustomFormControlScriptExecutorTest, ExtractsOptions) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        async extractOptions(node) {
          await new Promise(r => setTimeout(r, 0));
          return {
            options: [
              {text: node.id, value: 'v'},
              {text: 'no value'},
            ],
            popoverNeedsToBeClosed: true,
          };
        },
      };
    }
  )"));
  OptionsFuture future;
  executor_->ExtractOptions(GetTarget(), future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Take(),
            (CustomFormControlOptions{
                .options = {{.value = u"v", .text = u"target"},
                            {.value = u"no value", .text = u"no value"}},
                .popover_needs_to_be_closed = true,
            }));
}

// Tests that `ExtractOptions()` skips invalid entries, only treats `true` as
// true and defaults `value` to `text` if it isn't a string.
TEST_F(CustomFormControlScriptExecutorTest, ExtractOptionsSkipsInvalidEntries) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        extractOptions() {
          return {
            options: [
              null,
              {text: 'first'},
              42,
              {},
              {text: 42},
              {text: {toString() { return 'text'; }}},
              {get text() { throw new Error('error'); }},
              {text: 'text', get value() { throw new Error(); }},
              {text: 'last', value: 42},
            ],
            popoverNeedsToBeClosed: 1,
          };
        },
      };
    }
  )"));
  OptionsFuture future;
  executor_->ExtractOptions(GetTarget(), future.GetCallback());
  EXPECT_EQ(future.Take(),
            (CustomFormControlOptions{
                .options = {{.value = u"first", .text = u"first"},
                            {.value = u"last", .text = u"last"}},
            }));
}

// Tests that `ExtractOptions()` fails if there is no manager, if
// `extractOptions()` throws or its promise is rejected, or if the result isn't
// valid.
TEST_F(CustomFormControlScriptExecutorTest, ExtractOptionsFails) {
  LoadHTML("<div id=target></div>");
  OptionsFuture no_manager;
  executor_->ExtractOptions(GetTarget(), no_manager.GetCallback());
  EXPECT_EQ(no_manager.Take(), std::nullopt);

  for (const char* result : {
           "null",
           "{}",
           "{options: {length: 1, 0: {text: 'text'}}}",
           "{get options() { throw new Error('error'); }}",
           "{options: [], get popoverNeedsToBeClosed() { throw 1; }}",
           "Promise.reject(new Error('error'))",
           "(() => { throw new Error('error'); })()",
       }) {
    SCOPED_TRACE(result);
    ASSERT_TRUE(executor_->LoadScript(
        std::string("function createCustomFormControlManager() {"
                    "  return { extractOptions() { return ") +
        result + "; } };}"));
    OptionsFuture future;
    executor_->ExtractOptions(GetTarget(), future.GetCallback());
    EXPECT_EQ(future.Take(), std::nullopt);
  }
}

using SetValueResult = CustomFormControlSetValueResult;
using SetValueFuture = base::test::TestFuture<CustomFormControlSetValueResult>;

// Tests that `SetValue()` passes the element, the option's value and its text
// to `setValue()` and returns its result once its promise is fulfilled.
TEST_F(CustomFormControlScriptExecutorTest, SetsValue) {
  LoadHTML("<div id=target></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        async setValue(node, value, text) {
          await new Promise(r => setTimeout(r, 0));
          switch (value) {
            case 'missing': return {status: 'OPTION_NOT_FOUND'};
            case 'error': return {status: 'EXECUTION_ERROR'};
          }
          node.textContent = `${value}:${text}`;
          return {status: 'OK'};
        },
      };
    }
  )"));
  SetValueFuture future;
  executor_->SetValue(GetTarget(), {.value = u"AL", .text = u"Alabama"},
                      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Take(), SetValueResult::kOk);
  EXPECT_EQ(GetTarget().TextContent().Utf8(), "AL:Alabama");

  const struct {
    std::u16string value;
    SetValueResult result;
  } kTestCases[] = {
      {u"missing", SetValueResult::kOptionNotFound},
      {u"error", SetValueResult::kExecutionError},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(base::UTF16ToUTF8(test_case.value));
    executor_->SetValue(GetTarget(), {.value = test_case.value},
                        future.GetCallback());
    EXPECT_EQ(future.Take(), test_case.result);
  }
}

// Tests that `SetValue()` returns `kExecutionError` if there is no manager, if
// `setValue()` throws or its promise is rejected, or if the result isn't valid.
TEST_F(CustomFormControlScriptExecutorTest, SetValueFails) {
  LoadHTML("<div id=target></div>");
  SetValueFuture no_manager;
  executor_->SetValue(GetTarget(), {.value = u"value", .text = u"text"},
                      no_manager.GetCallback());
  EXPECT_EQ(no_manager.Take(), SetValueResult::kExecutionError);

  for (const char* result : {
           "null",
           "{}",
           "{status: 'ok'}",
           "{status: 0}",
           "{status: {toString() { return 'OK'; }}}",
           "{get status() { throw new Error('error'); }}",
           "Promise.reject(new Error('error'))",
           "(() => { throw new Error('error'); })()",
       }) {
    SCOPED_TRACE(result);
    ASSERT_TRUE(executor_->LoadScript(
        std::string("function createCustomFormControlManager() {"
                    "  return { setValue() { return ") +
        result + "; } };}"));
    SetValueFuture future;
    executor_->SetValue(GetTarget(), {.value = u"value", .text = u"text"},
                        future.GetCallback());
    EXPECT_EQ(future.Take(), SetValueResult::kExecutionError);
  }
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
// argument is a null node, or if the method throws, and that later calls still
// work.
TEST_F(CustomFormControlScriptExecutorTest, CallFails) {
  EXPECT_EQ(Call("anything"), std::nullopt);  // No manager.
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        notAFunction: 1,
        throws() { throw new Error('error'); },
        works() { return 'ok'; },
      };
    }
  )"));
  EXPECT_EQ(Call("missing"), std::nullopt);
  EXPECT_EQ(Call("notAFunction"), std::nullopt);
  EXPECT_EQ(Call("throws"), std::nullopt);
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
      content::RenderFrame::FromWebFrame(child), kWorldId);
  ASSERT_TRUE(child_executor.LoadScript(R"(
    function createCustomFormControlManager() {
      return { detach() { frameElement.remove(); return 'detached'; } };
    }
  )"));
  bool called = false;
  CustomFormControlScriptExecutorTestApi(&child_executor)
      .CallFunction(
          MayReturnPromise(true), "detach", {},
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
          MayReturnPromise(true), "anything", {},
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

// Tests that `DetectCustomFormControls()` extracts valid custom form controls.
TEST_F(CustomFormControlScriptExecutorTest, DetectCustomFormControls) {
  LoadHTML("<div id=c1></div><div id=c2></div><div id=c3></div>");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        detectCustomFormControls() {
          return [
            {
              canonicalDomNode: document.getElementById("c1"),
              controlType: "select",
              fieldType: "ADDRESS_HOME_COUNTRY",
              isActive: true,
            },
            {
              canonicalDomNode: document.getElementById("c2"),
              controlType: "select",
              fieldType: "ADDRESS_HOME_STATE",
              isActive: false,
              label: "State",
              currentValue: { text: "Bavaria", value: "BY" },
            },
            {
              canonicalDomNode: document.getElementById("c3"),
              controlType: "select",
              fieldType: "ADDRESS_HOME_CITY",
              isActive: true,
              currentValue: { text: "Munich" },
            },
          ];
        },
      };
    }
  )"));

  blink::WebDocument doc = GetMainFrame()->GetDocument();
  EXPECT_THAT(executor_->DetectCustomFormControls(),
              testing::ElementsAre(
                  CustomFormControl{
                      .canonical_dom_node = doc.GetElementById("c1"),
                      .field_type = u"ADDRESS_HOME_COUNTRY",
                      .is_active = true,
                  },
                  CustomFormControl{
                      .canonical_dom_node = doc.GetElementById("c2"),
                      .field_type = u"ADDRESS_HOME_STATE",
                      .is_active = false,
                      .label = u"State",
                      .current_option =
                          SelectOption{.value = u"BY", .text = u"Bavaria"},
                  },
                  CustomFormControl{
                      .canonical_dom_node = doc.GetElementById("c3"),
                      .field_type = u"ADDRESS_HOME_CITY",
                      .is_active = true,
                      .current_option =
                          SelectOption{.value = u"Munich", .text = u"Munich"},
                  }));
}

// Tests that `DetectCustomFormControls()` skips invalid controls and returns
// empty when no script is loaded.
TEST_F(CustomFormControlScriptExecutorTest,
       DetectCustomFormControlsSkipsInvalidControls) {
  EXPECT_TRUE(executor_->DetectCustomFormControls().empty());

  LoadHTML(R"(
    <div id=custom-select></div>
    <form id=form></form>
    <input id=text-input>
    <select id=select></select>
    <textarea id=textarea></textarea>
    <iframe id=iframe src="about:blank"></iframe>
  )");
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        detectCustomFormControls() {
          const base = {
            canonicalDomNode: document.getElementById("custom-select"),
            controlType: "select",
            fieldType: "ADDRESS_HOME_COUNTRY",
            isActive: true,
          };
          return [
            // Not an object.
            null,
            // Missing `canonicalDomNode`.
            { ...base, canonicalDomNode: null },
            // Detached `canonicalDomNode`.
            { ...base, canonicalDomNode: document.createElement("div") },
            // Cross-frame `canonicalDomNode`.
            {
              ...base,
              canonicalDomNode:
                  document.getElementById("iframe").contentDocument.body,
            },
            // `<form>` element.
            { ...base, canonicalDomNode: document.getElementById("form") },
            // Standard autofillable form control elements.
            {
              ...base,
              canonicalDomNode: document.getElementById("text-input"),
            },
            { ...base, canonicalDomNode: document.getElementById("select") },
            { ...base, canonicalDomNode: document.getElementById("textarea") },
            // Non-string `fieldType`.
            { ...base, fieldType: 123 },
            // Non-boolean `isActive`.
            { ...base, isActive: "true" },
            // `currentValue` missing `text`.
            { ...base, currentValue: {} },
            // Throwing property getter.
            { ...base, get label() { throw new Error("boom"); } },
          ];
        },
      };
    }
  )"));

  EXPECT_TRUE(executor_->DetectCustomFormControls().empty());
}

constexpr char kAsyncScript[] = R"(
  function createCustomFormControlManager() {
    return {
      async pending() { await null; return 'later'; },
      later() { return new Promise(r => setTimeout(() => r('later'), 0)); },
      rejectLater() {
        return new Promise((_, r) => setTimeout(() => r(new Error()), 0));
      },
      never() { return new Promise(() => {}); },
      async succeedNow() { return 'now'; },
      async failNow() { throw new Error(); },
      sync() { return 'sync'; },
    };
  }
)";

// Tests that calls that may return a promise wait for it and finish with the
// value it is fulfilled with, or fail if it is rejected.
TEST_F(CustomFormControlScriptExecutorTest, CallsWaitForPromises) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  CallResult later;
  StartCall(MayReturnPromise(true), "later", later);
  EXPECT_FALSE(later.done);
  ASSERT_TRUE(base::test::RunUntil([&] { return later.done; }));
  EXPECT_EQ(later.value, "later");

  CallResult rejected;
  StartCall(MayReturnPromise(true), "rejectLater", rejected);
  ASSERT_TRUE(base::test::RunUntil([&] { return rejected.done; }));
  EXPECT_TRUE(rejected.had_isolate);
  EXPECT_EQ(rejected.value, std::nullopt);
}

// Tests that calls that may not return a promise fail if they do.
TEST_F(CustomFormControlScriptExecutorTest, OnlySomeCallsMayReturnPromises) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  CallResult result;
  StartCall(MayReturnPromise(false), "pending", result);
  EXPECT_TRUE(result.done);
  EXPECT_EQ(result.value, std::nullopt);
}

// Tests that other calls fail while a call waits for a promise, and work again
// afterwards.
TEST_F(CustomFormControlScriptExecutorTest, CallsFailWhileWaiting) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  CallResult later;
  StartCall(MayReturnPromise(true), "later", later);
  CallResult sync;
  StartCall(MayReturnPromise(false), "sync", sync);
  EXPECT_TRUE(sync.done);
  EXPECT_EQ(sync.value, std::nullopt);
  ASSERT_TRUE(base::test::RunUntil([&] { return later.done; }));
  EXPECT_EQ(later.value, "later");
  EXPECT_EQ(Call("sync"), "sync");
}

// Tests that a call waiting for a promise fails when a new document is
// committed, since the promise can no longer settle. The frame is still alive,
// so the callback gets the isolate.
TEST_F(CustomFormControlScriptExecutorTest, FailsPendingCallOnNavigation) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  CallResult never;
  StartCall(MayReturnPromise(true), "never", never);
  EXPECT_FALSE(never.done);
  LoadHTML("<div>Another document</div>");
  EXPECT_TRUE(never.done);
  EXPECT_TRUE(never.had_isolate);
  EXPECT_EQ(never.value, std::nullopt);
}

// Tests that loading a script fails a call that waits for a promise of the
// previous manager, and that the promise settling afterwards is ignored.
TEST_F(CustomFormControlScriptExecutorTest, LoadScriptFailsPendingCall) {
  ASSERT_TRUE(executor_->LoadScript(R"(
    function createCustomFormControlManager() {
      return {
        later() {
          return new Promise(r => setTimeout(() => {
            window.resolved = true;
            r('later');
          }, 0));
        },
      };
    }
  )"));
  CallResult later;
  StartCall(MayReturnPromise(true), "later", later);
  EXPECT_FALSE(later.done);
  EXPECT_TRUE(executor_->LoadScript(kValidScript));
  EXPECT_TRUE(later.done);
  EXPECT_TRUE(later.had_isolate);
  EXPECT_EQ(later.value, std::nullopt);
  EXPECT_TRUE(executor_->has_manager());
  // The promise's reaction finds no waiting call.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return EvaluateInIsolatedWorld("String(window.resolved === true)") ==
           "true";
  }));
}

// Tests that calls finish right away if their promise is already fulfilled
// when the call returns.
TEST_F(CustomFormControlScriptExecutorTest, FinishesOnFulfilledPromise) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  CallResult result;
  StartCall(MayReturnPromise(true), "succeedNow", result);
  EXPECT_TRUE(result.done);
  EXPECT_EQ(result.value, "now");
}

// Tests that calls fail if their promise is already rejected when the call
// returns.
TEST_F(CustomFormControlScriptExecutorTest, FailsOnRejectedPromise) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  for (MayReturnPromise may_return_promise :
       {MayReturnPromise(true), MayReturnPromise(false)}) {
    CallResult result;
    StartCall(may_return_promise, "failNow", result);
    EXPECT_TRUE(result.done);
    EXPECT_TRUE(result.had_isolate);
    EXPECT_EQ(result.value, std::nullopt);
  }
}

// Tests that the callback of a call that waited for a promise may make another
// call.
TEST_F(CustomFormControlScriptExecutorTest, CallbackMayMakeAnotherCall) {
  ASSERT_TRUE(executor_->LoadScript(kAsyncScript));
  std::optional<std::string> nested;
  bool done = false;
  CustomFormControlScriptExecutorTestApi(executor_.get())
      .CallFunction(
          MayReturnPromise(true), "later", {},
          base::BindLambdaForTesting(
              [this, &nested, &done](v8::Isolate*, v8::Local<v8::Context>,
                                     v8::MaybeLocal<v8::Value>) {
                nested = Call("sync");
                done = true;
              }));
  ASSERT_TRUE(base::test::RunUntil([&] { return done; }));
  EXPECT_EQ(nested, "sync");
}

// Tests that a call waiting for a promise fails when its frame is detached, and
// that the callback still gets the isolate, since it runs while the frame's
// script contexts are released.
TEST_F(CustomFormControlScriptExecutorTest, FailsPendingCallOnFrameDetach) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child = GetMainFrame()->FirstChild()->ToWebLocalFrame();
  CustomFormControlScriptExecutor child_executor(
      content::RenderFrame::FromWebFrame(child), kWorldId);
  ASSERT_TRUE(child_executor.LoadScript(kAsyncScript));
  CallResult never;
  CustomFormControlScriptExecutorTestApi(&child_executor)
      .CallFunction(
          MayReturnPromise(true), "never", {},
          base::BindLambdaForTesting([&never](v8::Isolate* isolate,
                                              v8::Local<v8::Context> context,
                                              v8::MaybeLocal<v8::Value> value) {
            never.done = true;
            never.had_isolate = isolate;
            EXPECT_TRUE(value.IsEmpty());
          }));
  EXPECT_FALSE(never.done);
  ExecuteJavaScriptForTests("document.querySelector('iframe').remove();");
  EXPECT_TRUE(never.done);
  EXPECT_TRUE(never.had_isolate);
}

}  // namespace
}  // namespace autofill
