// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/renderer/actor/execute_js_v8_utils.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "gin/function_template.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

// Spins a run loop, as `window.print()` does when script calls it, and runs
// `task` in it.
void SpinRunLoop(const base::RepeatingClosure& task) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, task.Then(run_loop.QuitClosure()));
  run_loop.Run();
}

class ToolExecutorTest : public ChromeRenderViewTest {
 public:
  ToolExecutorTest() {
    feature_list_.InitAndEnableFeature(features::kGlicActor);
  }

 protected:
  static constexpr TaskId kTaskId{1};

  // Returns an invocation of an ExecuteJs tool that runs `script_code`, which
  // the browser can cancel through `canceller`.
  static mojom::ToolInvocationPtr MakeExecuteJsInvocation(
      std::string script_code,
      mojo::PendingReceiver<mojom::ExecuteJsCanceller> canceller = {}) {
    if (!canceller) {
      // The browser always sends one, even if it doesn't cancel the tool.
      std::ignore = canceller.InitWithNewPipeAndPassRemote();
    }
    auto invocation = mojom::ToolInvocation::New();
    invocation->task_id = kTaskId;
    invocation->action =
        mojom::ToolAction::NewExecuteJs(mojom::ExecuteJsAction::New(
            std::move(script_code), /*timeout_ms=*/0, std::move(canceller)));
    invocation->target = mojom::ToolTarget::NewDomNodeId(kRootElementDomNodeId);
    return invocation;
  }

  v8::Isolate* GetIsolate() {
    return GetMainRenderFrame()
        ->GetWebFrame()
        ->GetAgentGroupScheduler()
        ->Isolate();
  }

  // Returns the context of the actor's isolated world, where ExecuteJs scripts
  // run.
  v8::Local<v8::Context> GetActorContext() {
    return GetMainRenderFrame()->GetWebFrame()->GetScriptContextFromWorldId(
        GetIsolate(), ISOLATED_WORLD_ID_ACTOR);
  }

  // Returns `console.log` in `context`.
  v8::Local<v8::Value> GetConsoleLog(v8::Local<v8::Context> context) {
    v8::Isolate* isolate = GetIsolate();
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> console =
        context->Global()
            ->Get(context, v8::String::NewFromUtf8Literal(isolate, "console"))
            .ToLocalChecked();
    return console.As<v8::Object>()
        ->Get(context, v8::String::NewFromUtf8Literal(isolate, "log"))
        .ToLocalChecked();
  }

  // Defines the global function `name()` in `context`, which runs `callback`.
  void DefineFunction(v8::Local<v8::Context> context,
                      const char* name,
                      base::RepeatingClosure callback) {
    v8::Isolate* isolate = GetIsolate();
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> function =
        gin::CreateFunctionTemplate(isolate, std::move(callback))
            ->GetFunction(context)
            .ToLocalChecked();
    ASSERT_TRUE(
        context->Global()
            ->Set(context,
                  v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
                  function)
            .FromMaybe(false));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// While a `ScopedTimeoutWatchdog` watches script, e.g. an ExecuteJs script that
// spun a nested run loop in which its tool was cancelled, the executor doesn't
// start tools, since the termination of the watched script would hit their
// script instead.
TEST_F(ToolExecutorTest, BusyWhileScriptIsWatched) {
  Journal journal;
  ToolExecutor executor(GetMainRenderFrame(), journal);

  {
    ScopedTimeoutWatchdog watchdog(GetIsolate(), base::Seconds(30));
    base::test::TestFuture<mojom::ActionResultPtr> result;
    executor.InvokeTool(MakeExecuteJsInvocation("1 + 1"), result.GetCallback());
    EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kExecutorBusy);
  }

  base::test::TestFuture<mojom::ActionResultPtr> result;
  executor.InvokeTool(MakeExecuteJsInvocation("1 + 1"), result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kOk);
  ASSERT_TRUE(result.Get()->execute_js_response);
  EXPECT_EQ(result.Get()->execute_js_response->result, "2");
}

// In split execution, watched script can also start after `InitializeTool()`,
// e.g. in another frame that shares the isolate, and spin the nested run loop
// in which `ExecuteTool()` runs.
TEST_F(ToolExecutorTest, BusyWhileScriptIsWatchedAfterInitialize) {
  Journal journal;
  ToolExecutor executor(GetMainRenderFrame(), journal);

  ASSERT_TRUE(executor.InitializeTool(MakeExecuteJsInvocation("1 + 1"))
                  ->is_success_point());
  {
    ScopedTimeoutWatchdog watchdog(GetIsolate(), base::Seconds(30));
    base::test::TestFuture<mojom::ActionResultPtr> result;
    executor.ExecuteTool(kTaskId, result.GetCallback());
    EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kExecutorBusy);
  }

  ASSERT_TRUE(executor.InitializeTool(MakeExecuteJsInvocation("1 + 1"))
                  ->is_success_point());
  base::test::TestFuture<mojom::ActionResultPtr> result;
  executor.ExecuteTool(kTaskId, result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kOk);
  ASSERT_TRUE(result.Get()->execute_js_response);
  EXPECT_EQ(result.Get()->execute_js_response->result, "2");
}

// While the script blocks the main thread, the browser cancels the tool through
// `ExecuteJsCanceller`, which the renderer handles off the main thread. The
// tool then reports that it was cancelled, rather than that it timed out.
TEST_F(ToolExecutorTest, ExecuteJsCancellerStopsBlockingScript) {
  Journal journal;
  ToolExecutor executor(GetMainRenderFrame(), journal);

  mojo::Remote<mojom::ExecuteJsCanceller> canceller;
  mojom::ToolInvocationPtr invocation = MakeExecuteJsInvocation(
      "while (true) {}", canceller.BindNewPipeAndPassReceiver());
  // Time is mocked, so the script's timeout can't fire while it blocks the main
  // thread: only the cancellation stops it, whether the renderer handles it
  // before the script starts or while it runs.
  canceller->Cancel();
  base::test::TestFuture<mojom::ActionResultPtr> result;
  executor.InvokeTool(std::move(invocation), result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kInvokeCanceled);
  EXPECT_FALSE(GetIsolate()->IsExecutionTerminating());

  base::test::TestFuture<mojom::ActionResultPtr> follow_up_result;
  executor.InvokeTool(MakeExecuteJsInvocation("1 + 1"),
                      follow_up_result.GetCallback());
  EXPECT_EQ(follow_up_result.Get()->code, mojom::ActionResultCode::kOk);
  ASSERT_TRUE(follow_up_result.Get()->execute_js_response);
  EXPECT_EQ(follow_up_result.Get()->execute_js_response->result, "2");
}

// The script can spin a run loop, e.g. by calling `window.print()`, in which
// the browser cancels the tool. Cancelling it there restores the console, and
// terminates the script once the run loop returns to it.
TEST_F(ToolExecutorTest, CancelToolWhileScriptSpinsRunLoop) {
  Journal journal;
  ToolExecutor executor(GetMainRenderFrame(), journal);

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetActorContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> console_log = GetConsoleLog(context);
  DefineFunction(
      context, "spinRunLoop",
      base::BindRepeating(&SpinRunLoop, base::BindLambdaForTesting([&] {
        executor.CancelTool(kTaskId);
      })));

  // Time is mocked, so the script's timeout can't fire while it blocks the main
  // thread: only the termination that cancelling the tool requests stops it.
  base::test::TestFuture<mojom::ActionResultPtr> result;
  executor.InvokeTool(MakeExecuteJsInvocation("spinRunLoop(); while (true) {}"),
                      result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kInvokeCanceled);
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  EXPECT_TRUE(GetConsoleLog(context)->StrictEquals(console_log));

  base::test::TestFuture<mojom::ActionResultPtr> follow_up_result;
  executor.InvokeTool(MakeExecuteJsInvocation("1 + 1"),
                      follow_up_result.GetCallback());
  EXPECT_EQ(follow_up_result.Get()->code, mojom::ActionResultCode::kOk);
  ASSERT_TRUE(follow_up_result.Get()->execute_js_response);
  EXPECT_EQ(follow_up_result.Get()->execute_js_response->result, "2");
}

}  // namespace
}  // namespace actor
