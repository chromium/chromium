// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_console_capture.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "chrome/renderer/actor/execute_js_v8_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

class ExecuteJsConsoleCaptureTest : public ChromeRenderViewTest {
 protected:
  v8::Isolate* GetIsolate() {
    return GetMainRenderFrame()
        ->GetWebFrame()
        ->GetAgentGroupScheduler()
        ->Isolate();
  }

  v8::Local<v8::Value> GetConsoleLog(v8::Isolate* isolate,
                                     v8::Local<v8::Context> context) {
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> console_val =
        context->Global()
            ->Get(context, v8::String::NewFromUtf8Literal(isolate, "console"))
            .ToLocalChecked();
    return console_val.As<v8::Object>()
        ->Get(context, v8::String::NewFromUtf8Literal(isolate, "log"))
        .ToLocalChecked();
  }

  // Compiles and executes `code` in `context`. Returns true on normal
  // completion, or false if an exception or termination occurred.
  bool RunScript(v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 const char* code) {
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, code).ToLocalChecked();
    v8::Local<v8::Script> script;
    v8::Local<v8::Value> value;
    return v8::Script::Compile(context, source).ToLocal(&script) &&
           script->Run(context).ToLocal(&value);
  }
};

TEST_F(ExecuteJsConsoleCaptureTest,
       TakeConsoleLogsConsumesCaptureAndRestoresConsole) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      GetMainRenderFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Value> orig_log = GetConsoleLog(isolate, context);
  std::unique_ptr<ConsoleCapture> capture = ConsoleCapture::Install(
      isolate, context, base::TimeTicks::Now() + base::Seconds(30));
  ASSERT_TRUE(capture);
  EXPECT_FALSE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));

  ASSERT_TRUE(RunScript(isolate, context, "console.log('hello');"));

  std::string logs = TakeConsoleLogs(std::move(capture));
  EXPECT_EQ(capture, nullptr);
  EXPECT_EQ(logs, "[log] hello");
  EXPECT_EQ(TakeConsoleLogs(nullptr), "");
  EXPECT_TRUE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));
}

TEST_F(ExecuteJsConsoleCaptureTest,
       ScopedActiveWatchdogCompletesDeferredRestoreAfterTermination) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      GetMainRenderFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Value> orig_log = GetConsoleLog(isolate, context);

  std::optional<ConsoleCapture::ScopedActiveWatchdog> active_watchdog_guard;
  {
    std::unique_ptr<ConsoleCapture> capture = ConsoleCapture::Install(
        isolate, context, base::TimeTicks::Now() + base::Seconds(30));
    ASSERT_TRUE(capture);
    active_watchdog_guard.emplace(capture.get());
    EXPECT_FALSE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));

    ScopedTimeoutWatchdog watchdog(isolate, base::TimeDelta());
    // Wait for the watchdog to request V8 termination, observe the termination
    // interrupt in V8, then destroy `capture` while terminating (simulating
    // Cancel() / ~ExecuteJsTool() inside a nested run loop racing with
    // timeout).
    base::ThreadPoolInstance::Get()->FlushForTesting();
    EXPECT_FALSE(RunScript(isolate, context, "1 + 1"));
    ASSERT_TRUE(try_catch.HasTerminated());
    ASSERT_TRUE(isolate->IsExecutionTerminating());
    capture.reset();

    EXPECT_TRUE(watchdog.Stop());
    EXPECT_FALSE(isolate->IsExecutionTerminating());
    // Restoration was deferred while `active_watchdog_guard` stayed in scope.
    EXPECT_FALSE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));
  }

  // `active_watchdog_guard` outlives `watchdog` and completes the deferred
  // restoration once termination has been cancelled.
  active_watchdog_guard.reset();
  EXPECT_TRUE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));
}

TEST_F(ExecuteJsConsoleCaptureTest,
       ScopedActiveWatchdogReleasesHandlesOnExternalTermination) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      GetMainRenderFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  ASSERT_TRUE(RunScript(
      isolate, context,
      "globalThis.__logged = [];"
      "console.log = (...args) => globalThis.__logged.push(args.join(' '));"));
  v8::Local<v8::Value> orig_log = GetConsoleLog(isolate, context);
  std::unique_ptr<ConsoleCapture> capture = ConsoleCapture::Install(
      isolate, context, base::TimeTicks::Now() + base::Seconds(30));
  ASSERT_TRUE(capture);
  std::optional<ConsoleCapture::ScopedActiveWatchdog> active_watchdog_guard(
      std::in_place, capture.get());

  // Simulate an external V8 termination that is not cancelled before
  // `active_watchdog_guard` unwinds.
  isolate->TerminateExecution();
  EXPECT_FALSE(RunScript(isolate, context, "1 + 1"));
  ASSERT_TRUE(try_catch.HasTerminated());
  ASSERT_TRUE(isolate->IsExecutionTerminating());
  capture.reset();
  active_watchdog_guard.reset();

  isolate->CancelTerminateExecution();
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  // Because termination was still active when `active_watchdog_guard` unwound,
  // handles were released without attempting V8 restoration, and calling the
  // orphaned wrapper afterwards remains a safe pass-through to `orig_log`.
  EXPECT_FALSE(GetConsoleLog(isolate, context)->StrictEquals(orig_log));
  ASSERT_TRUE(RunScript(isolate, context, "console.log('after_cancel');"));
  EXPECT_TRUE(
      RunScript(isolate, context,
                "if (globalThis.__logged.length !== 1 ||"
                "    globalThis.__logged[0] !== 'after_cancel') {"
                "  throw new Error('spy did not receive forwarded call');"
                "}"));
}

}  // namespace
}  // namespace actor
