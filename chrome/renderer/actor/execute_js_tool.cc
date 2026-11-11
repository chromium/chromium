// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_tool.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

// Maximum size, in bytes, of the message describing a script failure.
constexpr size_t kMaxErrorMessageBytes = 1024;

// State shared by a ScopedTimeoutWatchdog and its timeout task, which runs on
// the thread pool. The lock makes terminating and cancelling mutually
// exclusive, so script is never terminated after the watchdog is cancelled.
class WatchdogState : public base::RefCountedThreadSafe<WatchdogState> {
 public:
  explicit WatchdogState(v8::Isolate* isolate) : isolate_(isolate) {}

  WatchdogState(const WatchdogState&) = delete;
  WatchdogState& operator=(const WatchdogState&) = delete;

  void OnTimeout() {
    base::AutoLock lock(lock_);
    if (cancelled_) {
      return;
    }
    terminated_ = true;
    isolate_->TerminateExecution();
  }

  // Once this returns, OnTimeout() no longer terminates execution. Returns
  // whether it already did.
  bool Cancel() {
    base::AutoLock lock(lock_);
    cancelled_ = true;
    return terminated_;
  }

 private:
  friend class base::RefCountedThreadSafe<WatchdogState>;
  ~WatchdogState() = default;

  base::Lock lock_;
  // Only used before Cancel(), which the owning ScopedTimeoutWatchdog calls
  // while the isolate is alive.
  const raw_ptr<v8::Isolate> isolate_;
  bool cancelled_ GUARDED_BY(lock_) = false;
  bool terminated_ GUARDED_BY(lock_) = false;
};

// Terminates JavaScript running on `isolate` once `timeout` elapses. Scope it
// to the call that runs page JavaScript: stopping or destroying it stops the
// timeout and clears any termination it requested, so the termination can't
// hit unrelated script that runs later.
class ScopedTimeoutWatchdog {
 public:
  ScopedTimeoutWatchdog(v8::Isolate* isolate, base::TimeDelta timeout)
      : isolate_(isolate),
        state_(base::MakeRefCounted<WatchdogState>(isolate)) {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&WatchdogState::OnTimeout, state_), timeout);
  }

  ScopedTimeoutWatchdog(const ScopedTimeoutWatchdog&) = delete;
  ScopedTimeoutWatchdog& operator=(const ScopedTimeoutWatchdog&) = delete;

  ~ScopedTimeoutWatchdog() { Stop(); }

  // Returns whether the watchdog terminated script.
  bool Stop() {
    if (!stopped_) {
      stopped_ = true;
      terminated_ = state_->Cancel();
      if (terminated_) {
        isolate_->CancelTerminateExecution();
      }
    }
    return terminated_;
  }

 private:
  const raw_ptr<v8::Isolate> isolate_;
  const scoped_refptr<WatchdogState> state_;
  bool stopped_ = false;
  bool terminated_ = false;
};

// A wait for the promise a script returned. Promise reactions can run after
// the tool is gone, so they look the wait up by ID rather than holding a
// pointer. The tool removes its wait when it is destroyed.
struct PendingPromise {
  ExecuteJsTool::ToolFinishedCallback callback;
  base::TimeTicks deadline;
};

// Only accessed on the render main thread.
base::flat_map<uint32_t, PendingPromise>& GetPendingPromises() {
  static base::NoDestructor<base::flat_map<uint32_t, PendingPromise>>
      pending_promises;
  return *pending_promises;
}

uint32_t AddPendingPromise(PendingPromise pending_promise) {
  static uint32_t next_id = 0;
  // 0 means that a tool has no pending promise.
  if (++next_id == 0) {
    ++next_id;
  }
  GetPendingPromises().emplace(next_id, std::move(pending_promise));
  return next_id;
}

std::optional<PendingPromise> TakePendingPromise(uint32_t id) {
  auto& pending_promises = GetPendingPromises();
  auto it = pending_promises.find(id);
  if (it == pending_promises.end()) {
    return std::nullopt;
  }
  PendingPromise pending_promise = std::move(it->second);
  pending_promises.erase(it);
  return pending_promise;
}

// The browser already clamps the timeout. Clamp it again so that a bad value
// can't block the main thread for longer.
base::TimeDelta GetTimeout(uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return kExecuteJsDefaultTimeout;
  }
  return std::min(base::Milliseconds(timeout_ms), kExecuteJsMaxTimeout);
}

struct ScriptValueText {
  std::string text;
  bool truncated = false;
};

// Returns `str` as UTF-8, truncated to at most `max_bytes` bytes.
ScriptValueText ToUtf8(v8::Isolate* isolate,
                       v8::Local<v8::String> str,
                       size_t max_bytes) {
  const size_t length = str->Utf8Length(isolate);
  ScriptValueText utf8;
  utf8.text.resize(std::min(length, max_bytes));
  utf8.text.resize(str->WriteUtf8(isolate, utf8.text.data(), utf8.text.size(),
                                  v8::String::WriteFlags::kReplaceInvalidUtf8));
  utf8.truncated = utf8.text.size() < length;
  return utf8;
}

// Converts `value`, which a script produced, to text: JSON if it can be
// serialized, and its string conversion otherwise. Both can run page
// JavaScript (toJSON(), toString()), so the conversion is bounded by
// `deadline`. Returns std::nullopt if the deadline passes first.
std::optional<ScriptValueText> ConvertResult(v8::Isolate* isolate,
                                             v8::Local<v8::Context> context,
                                             v8::Local<v8::Value> value,
                                             base::TimeTicks deadline) {
  if (value->IsUndefined()) {
    return ScriptValueText();
  }
  const base::TimeDelta remaining = deadline - base::TimeTicks::Now();
  if (!remaining.is_positive()) {
    return std::nullopt;
  }
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  ScopedTimeoutWatchdog watchdog(isolate, remaining);
  v8::Local<v8::String> text;
  if (!v8::JSON::Stringify(context, value).ToLocal(&text)) {
    if (try_catch.HasTerminated()) {
      return std::nullopt;
    }
    try_catch.Reset();
    if (!value->ToString(context).ToLocal(&text)) {
      if (try_catch.HasTerminated()) {
        return std::nullopt;
      }
      return ScriptValueText();
    }
  }
  return ToUtf8(isolate, text, kExecuteJsMaxResultBytes);
}

// Describes `error`, a thrown exception or a rejection reason, for the failure
// message. Its string conversion can run page JavaScript, so it is bounded by
// `deadline`.
std::string DescribeError(v8::Isolate* isolate,
                          v8::Local<v8::Context> context,
                          v8::Local<v8::Value> error,
                          base::TimeTicks deadline) {
  static constexpr char kUnknownError[] = "The script failed.";
  const base::TimeDelta remaining = deadline - base::TimeTicks::Now();
  if (!remaining.is_positive()) {
    return kUnknownError;
  }
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  ScopedTimeoutWatchdog watchdog(isolate, remaining);
  v8::Local<v8::String> text;
  if (!error->ToString(context).ToLocal(&text)) {
    return kUnknownError;
  }
  return ToUtf8(isolate, text, kMaxErrorMessageBytes).text;
}

mojom::ActionResultPtr MakeTimeoutResult(std::string_view message) {
  return MakeResult(mojom::ActionResultCode::kExecuteJsTimeout,
                    /*requires_page_stabilization=*/false, message);
}

mojom::ActionResultPtr MakeScriptFailedResult(std::string_view message) {
  return MakeResult(mojom::ActionResultCode::kExecuteJsScriptFailed,
                    /*requires_page_stabilization=*/false, message);
}

// Returns the result for a script whose promise was rejected with `error`. The
// description of `error` is both the failure message and `js_error_message`.
mojom::ActionResultPtr MakeJsErrorResult(v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::Local<v8::Value> error,
                                         base::TimeTicks deadline) {
  std::string description = DescribeError(isolate, context, error, deadline);
  mojom::ActionResultPtr result = MakeScriptFailedResult(description);
  result->execute_js_response = mojom::ExecuteJsResponse::New(
      /*result=*/std::string(), /*truncated=*/false, std::move(description));
  return result;
}

// Returns the result for a script that produced `value`.
mojom::ActionResultPtr MakeValueResult(v8::Isolate* isolate,
                                       v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value,
                                       base::TimeTicks deadline) {
  std::optional<ScriptValueText> text =
      ConvertResult(isolate, context, value, deadline);
  if (!text) {
    return MakeTimeoutResult("Timed out converting the script result.");
  }
  mojom::ActionResultPtr result =
      MakeOkResult(/*requires_page_stabilization=*/false);
  result->execute_js_response =
      mojom::ExecuteJsResponse::New(std::move(text->text), text->truncated,
                                    /*js_error_message=*/std::nullopt);
  return result;
}

void OnPromiseSettled(const v8::FunctionCallbackInfo<v8::Value>& info,
                      bool fulfilled) {
  std::optional<PendingPromise> pending_promise =
      TakePendingPromise(info.DataV2().As<v8::Uint32>()->Value());
  if (!pending_promise) {
    // The tool already finished: it timed out or was cancelled.
    return;
  }
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  mojom::ActionResultPtr result =
      fulfilled ? MakeValueResult(isolate, context, info[0],
                                  pending_promise->deadline)
                : MakeJsErrorResult(isolate, context, info[0],
                                    pending_promise->deadline);
  std::move(pending_promise->callback).Run(std::move(result));
}

void OnPromiseFulfilled(const v8::FunctionCallbackInfo<v8::Value>& info) {
  OnPromiseSettled(info, /*fulfilled=*/true);
}

void OnPromiseRejected(const v8::FunctionCallbackInfo<v8::Value>& info) {
  OnPromiseSettled(info, /*fulfilled=*/false);
}

}  // namespace

ExecuteJsTool::ExecuteJsTool(content::RenderFrame& frame,
                             TaskId task_id,
                             Journal& journal,
                             mojom::ToolTargetPtr target,
                             mojom::ObservedToolTargetPtr observed_target,
                             mojom::ExecuteJsActionPtr action)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

ExecuteJsTool::~ExecuteJsTool() {
  if (pending_promise_id_) {
    GetPendingPromises().erase(pending_promise_id_);
  }
}

ValidationResult ExecuteJsTool::Validate() {
  if (!action_ || action_->script_code.empty()) {
    return ValidationResult(MakeResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        /*requires_page_stabilization=*/false, "Script code cannot be empty."));
  }
  return ValidationResult(MakeOkResult(/*requires_page_stabilization=*/false));
}

void ExecuteJsTool::Execute(ToolFinishedCallback callback) {
  blink::WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kFrameWentAway,
                                       /*requires_page_stabilization=*/false,
                                       "RenderFrame is no longer valid."));
    return;
  }

  // The script can spin a nested run loop (e.g. by calling window.print()) in
  // which this tool is cancelled and destroyed. Copy what is needed after the
  // script runs, and check `weak_this` once it returns.
  const base::TimeDelta timeout = GetTimeout(action_->timeout_ms);
  const base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  const blink::WebScriptSource source(
      blink::WebString::FromUtf8(action_->script_code));
  base::WeakPtr<ExecuteJsTool> weak_this = weak_ptr_factory_.GetWeakPtr();

  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> value;
  bool timed_out = false;
  {
    // Also bounds the microtasks the script queues, since they run before
    // script execution returns. Later continuations, e.g. after a timer or a
    // fetch, run as regular page tasks.
    ScopedTimeoutWatchdog watchdog(isolate, timeout);
    value = web_frame->ExecuteScriptInIsolatedWorldAndReturnValue(
        ISOLATED_WORLD_ID_ACTOR, source, blink::BackForwardCacheAware::kAllow);
    timed_out = watchdog.Stop() || try_catch.HasTerminated();
  }

  if (timed_out) {
    std::move(callback).Run(MakeTimeoutResult("The script timed out."));
    return;
  }
  if (!weak_this) {
    // `callback` is bound to a WeakPtr to the ToolExecutor, which was
    // invalidated when this tool was cancelled or the frame went away.
    return;
  }

  v8::Local<v8::Context> context =
      web_frame->GetScriptContextFromWorldId(isolate, ISOLATED_WORLD_ID_ACTOR);
  if (context.IsEmpty()) {
    std::move(callback).Run(
        MakeScriptFailedResult("The script context is no longer available."));
    return;
  }

  if (value.IsEmpty()) {
    // Blink reports exceptions the script throws, including syntax errors, to
    // the console rather than to the caller, so no details are available.
    std::move(callback).Run(MakeScriptFailedResult(
        "The script threw an exception or has a syntax error."));
    return;
  }

  if (value->IsPromise()) {
    v8::Local<v8::Promise> promise = value.As<v8::Promise>();
    switch (promise->State()) {
      case v8::Promise::kFulfilled:
        value = promise->Result();
        break;
      case v8::Promise::kRejected:
        std::move(callback).Run(
            MakeJsErrorResult(isolate, context, promise->Result(), deadline));
        return;
      case v8::Promise::kPending: {
        const uint32_t id = AddPendingPromise({std::move(callback), deadline});
        v8::Context::Scope context_scope(context);
        v8::Local<v8::Value> data = v8::Integer::NewFromUnsigned(isolate, id);
        v8::Local<v8::Function> on_fulfilled;
        v8::Local<v8::Function> on_rejected;
        if (!v8::Function::New(context, OnPromiseFulfilled, data)
                 .ToLocal(&on_fulfilled) ||
            !v8::Function::New(context, OnPromiseRejected, data)
                 .ToLocal(&on_rejected) ||
            promise->Then(context, on_fulfilled, on_rejected).IsEmpty()) {
          // Only happens while script execution is being terminated.
          std::optional<PendingPromise> pending_promise =
              TakePendingPromise(id);
          std::move(pending_promise->callback)
              .Run(MakeScriptFailedResult(
                  "Failed to wait for the promise the script returned."));
          return;
        }
        pending_promise_id_ = id;
        promise_timeout_timer_.Start(
            FROM_HERE, deadline - base::TimeTicks::Now(),
            base::BindOnce(&ExecuteJsTool::OnPromiseTimeout,
                           base::Unretained(this)));
        return;
      }
    }
  }

  std::move(callback).Run(MakeValueResult(isolate, context, value, deadline));
}

void ExecuteJsTool::Cancel() {}

std::string ExecuteJsTool::DebugString() const {
  return absl::StrFormat(
      "ExecuteJsTool[script_code(%s)]",
      action_ ? base::TruncateUTF8ToByteSize(action_->script_code,
                                             kExecuteJsMaxLoggedScriptBytes)
              : std::string_view());
}

void ExecuteJsTool::OnPromiseTimeout() {
  std::optional<PendingPromise> pending_promise =
      TakePendingPromise(pending_promise_id_);
  pending_promise_id_ = 0;
  if (!pending_promise) {
    return;
  }
  std::move(pending_promise->callback)
      .Run(MakeTimeoutResult(
          "Timed out waiting for the promise the script returned."));
}

}  // namespace actor
