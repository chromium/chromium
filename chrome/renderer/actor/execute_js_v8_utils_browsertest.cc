// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_v8_utils.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
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
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

class ExecuteJsV8UtilsTest : public ChromeRenderViewTest {
 protected:
  v8::Isolate* GetIsolate() {
    return GetMainRenderFrame()
        ->GetWebFrame()
        ->GetAgentGroupScheduler()
        ->Isolate();
  }

  // Runs `1 + 1` in the main world and returns the integer result, or
  // `std::nullopt` if V8 execution was terminated.
  std::optional<int> EvalOnePlusOne(bool* terminated) {
    *terminated = false;
    v8::Isolate* isolate = GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context =
        GetMainRenderFrame()->GetWebFrame()->MainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8Literal(isolate, "1 + 1");
    v8::Local<v8::Script> script;
    v8::Local<v8::Value> value;
    if (!v8::Script::Compile(context, source).ToLocal(&script) ||
        !script->Run(context).ToLocal(&value)) {
      *terminated = try_catch.HasTerminated();
      return std::nullopt;
    }
    return value->Int32Value(context).FromMaybe(-1);
  }
};

TEST_F(ExecuteJsV8UtilsTest, OverlappingWatchdogsTrackActiveState) {
  v8::Isolate* isolate = GetIsolate();
  EXPECT_FALSE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));

  ScopedTimeoutWatchdog outer(isolate, base::Seconds(30));
  EXPECT_TRUE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));
  {
    ScopedTimeoutWatchdog inner(isolate, base::Seconds(30));
    EXPECT_TRUE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));
    EXPECT_FALSE(inner.Stop());
    EXPECT_TRUE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));
  }
  EXPECT_TRUE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));
  EXPECT_FALSE(outer.Stop());
  EXPECT_FALSE(ScopedTimeoutWatchdog::HasActiveWatchdog(isolate));
}

TEST_F(ExecuteJsV8UtilsTest, InnerWatchdogDoesNotCancelOuterTermination) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch outer_try_catch(isolate);

  ScopedTimeoutWatchdog outer(isolate, base::TimeDelta());
  ScopedTimeoutWatchdog inner(isolate, base::TimeDelta());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Both watchdogs fired. Stopping `inner` must not cancel `outer`'s pending
  // termination.
  EXPECT_TRUE(inner.Stop());
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), std::nullopt);
  EXPECT_TRUE(terminated);
  EXPECT_TRUE(outer_try_catch.HasTerminated());
  EXPECT_TRUE(isolate->IsExecutionTerminating());

  // Once `outer` stops, termination is cleared and script executes normally.
  EXPECT_TRUE(outer.Stop());
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  EXPECT_FALSE(outer_try_catch.HasTerminated());
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

TEST_F(ExecuteJsV8UtilsTest,
       InnerWatchdogCancelsOwnTerminationWhenOuterHasNotTimedOut) {
  v8::Isolate* isolate = GetIsolate();

  ScopedTimeoutWatchdog outer(isolate, base::Seconds(30));
  ScopedTimeoutWatchdog inner(isolate, base::TimeDelta());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Only `inner` fired; stopping it cancels termination so `outer`'s script can
  // continue running.
  EXPECT_TRUE(inner.Stop());
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);

  EXPECT_FALSE(outer.Stop());
}

TEST_F(ExecuteJsV8UtilsTest,
       OuterTerminationSurvivesInnerStopWhenInnerDidNotFire) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch outer_try_catch(isolate);

  ScopedTimeoutWatchdog outer(isolate, base::TimeDelta());
  ScopedTimeoutWatchdog inner(isolate, base::Seconds(30));
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_FALSE(inner.Stop());
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), std::nullopt);
  EXPECT_TRUE(terminated);
  EXPECT_TRUE(outer_try_catch.HasTerminated());
  EXPECT_TRUE(isolate->IsExecutionTerminating());

  EXPECT_TRUE(outer.Stop());
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  EXPECT_FALSE(outer_try_catch.HasTerminated());
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

TEST_F(ExecuteJsV8UtilsTest, CancellerTerminatesWatchedScript) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch outer_try_catch(isolate);
  auto canceller = base::MakeRefCounted<ScriptCanceller>();

  ScopedTimeoutWatchdog watchdog(isolate, base::Seconds(30), canceller);
  canceller->Cancel();
  EXPECT_TRUE(canceller->IsCancelled());
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), std::nullopt);
  EXPECT_TRUE(terminated);

  EXPECT_TRUE(watchdog.Stop());
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  EXPECT_FALSE(outer_try_catch.HasTerminated());
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

// A watchdog for a canceller that was already cancelled terminates script right
// away.
TEST_F(ExecuteJsV8UtilsTest, CancelledCancellerTerminatesScriptOnceWatched) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch outer_try_catch(isolate);
  auto canceller = base::MakeRefCounted<ScriptCanceller>();
  canceller->Cancel();

  ScopedTimeoutWatchdog watchdog(isolate, base::Seconds(30), canceller);
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), std::nullopt);
  EXPECT_TRUE(terminated);

  EXPECT_TRUE(watchdog.Stop());
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

// Once no watchdog watches script for the canceller, cancelling doesn't
// terminate unrelated script.
TEST_F(ExecuteJsV8UtilsTest, CancellerDoesNotTerminateUnwatchedScript) {
  v8::Isolate* isolate = GetIsolate();
  auto canceller = base::MakeRefCounted<ScriptCanceller>();
  {
    ScopedTimeoutWatchdog watchdog(isolate, base::Seconds(30), canceller);
    EXPECT_FALSE(watchdog.Stop());
  }

  canceller->Cancel();
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

// Cancelling terminates script as the watching watchdog, so an inner watchdog
// that also terminated script doesn't cancel the termination when it stops.
TEST_F(ExecuteJsV8UtilsTest, InnerWatchdogDoesNotCancelCancellation) {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch outer_try_catch(isolate);
  auto canceller = base::MakeRefCounted<ScriptCanceller>();

  ScopedTimeoutWatchdog outer(isolate, base::Seconds(30), canceller);
  ScopedTimeoutWatchdog inner(isolate, base::TimeDelta());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  canceller->Cancel();

  EXPECT_TRUE(inner.Stop());
  bool terminated = false;
  EXPECT_EQ(EvalOnePlusOne(&terminated), std::nullopt);
  EXPECT_TRUE(terminated);
  EXPECT_TRUE(isolate->IsExecutionTerminating());

  EXPECT_TRUE(outer.Stop());
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  EXPECT_FALSE(outer_try_catch.HasTerminated());
  EXPECT_EQ(EvalOnePlusOne(&terminated), 2);
  EXPECT_FALSE(terminated);
}

}  // namespace
}  // namespace actor
