// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_tool.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/renderer/actor/execute_js_console_capture.h"
#include "chrome/renderer/actor/execute_js_v8_utils.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

// A wait for the promise a script returned. Promise reactions can run after
// the tool is gone, so they look the wait up by ID rather than holding a
// pointer. The tool removes its wait when it is destroyed.
struct PendingPromise {
  ExecuteJsTool::ToolFinishedCallback callback;
  base::TimeTicks deadline;
  std::unique_ptr<ConsoleCapture> console_capture;
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

// Converts `value`, which a script produced, to text: JSON if it can be
// serialized (after normalizing DOM/built-in objects), and its string
// conversion otherwise. Both can run JavaScript (normalization, toJSON(),
// toString()), so the conversion is bounded by `deadline`. Returns std::nullopt
// if the deadline passes first.
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
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  ScopedTimeoutWatchdog watchdog(isolate, remaining);
  v8::Local<v8::Value> normalized = value;
  if (value->IsBigInt() || (value->IsObject() && !value->IsFunction())) {
    if (!NormalizeResultValue(isolate, context, value).ToLocal(&normalized)) {
      if (try_catch.HasTerminated()) {
        return std::nullopt;
      }
      try_catch.Reset();
      normalized = value;
    }
  }
  v8::Local<v8::String> text;
  if (!v8::JSON::Stringify(context, normalized).ToLocal(&text)) {
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

// Returns `str` as a V8 string, or empty if it exceeds V8's length limit.
v8::MaybeLocal<v8::String> ToV8String(v8::Isolate* isolate,
                                      std::string_view str) {
  if (!base::IsValueInRangeForNumericType<int>(str.size())) {
    return {};
  }
  return v8::String::NewFromUtf8(isolate, str.data(),
                                 v8::NewStringType::kNormal,
                                 static_cast<int>(str.size()));
}

// Formats the `v8::Message` and 1-based line number from `try_catch`, adjusting
// line numbers by `line_offset` wrapper lines prepended before user code. Does
// not run user JavaScript.
std::optional<std::string> FormatTryCatchMessage(v8::Isolate* isolate,
                                                 v8::Local<v8::Context> context,
                                                 const v8::TryCatch& try_catch,
                                                 int line_offset) {
  v8::Local<v8::Message> message = try_catch.Message();
  if (message.IsEmpty()) {
    return std::nullopt;
  }
  v8::Local<v8::String> message_text = message->Get();
  if (message_text.IsEmpty()) {
    return std::nullopt;
  }
  std::string suffix;
  int line = 0;
  if (message->GetLineNumber(context).To(&line)) {
    line -= line_offset;
    if (line > 0) {
      suffix = absl::StrFormat(" (line %d)", line);
    }
  }
  const size_t max_message_bytes =
      kExecuteJsMaxErrorMessageBytes > suffix.size()
          ? kExecuteJsMaxErrorMessageBytes - suffix.size()
          : 0;
  std::string text = ToUtf8(isolate, message_text, max_message_bytes).text;
  text.append(suffix);
  return text;
}

struct CompiledScript {
  v8::Local<v8::Script> script;
  // Number of wrapper lines prepended before the caller's script code.
  int line_offset = 0;
  // Populated when all compilation attempts fail without termination.
  std::string error_message;
};

// Compiles `script_code` as a classic script first. On a syntax error (e.g.
// bare `return`, top-level `await`, or an unparenthesized object literal),
// retries wrapped as an async arrow expression and then an async arrow block.
CompiledScript CompileScript(v8::Isolate* isolate,
                             v8::Local<v8::Context> context,
                             std::string_view script_code,
                             bool* terminated) {
  auto try_compile = [&](std::string_view code, int line_offset,
                         v8::Local<v8::Script>* out_script,
                         std::string* out_error) -> bool {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> v8_source;
    if (!ToV8String(isolate, code).ToLocal(&v8_source)) {
      if (try_catch.HasTerminated()) {
        *terminated = true;
        return false;
      }
      *out_error =
          FormatTryCatchMessage(isolate, context, try_catch, line_offset)
              .value_or("Failed to convert script code to a V8 string.");
      return false;
    }
    if (v8::Script::Compile(context, v8_source).ToLocal(out_script)) {
      return true;
    }
    if (try_catch.HasTerminated()) {
      *terminated = true;
      return false;
    }
    *out_error = FormatTryCatchMessage(isolate, context, try_catch, line_offset)
                     .value_or("The script failed to compile.");
    return false;
  };

  CompiledScript result;
  std::string raw_error;
  if (try_compile(script_code, /*line_offset=*/0, &result.script, &raw_error) ||
      *terminated) {
    return result;
  }

  // Try an async arrow expression first so bare `await <expr>` and object
  // literals like `{a: 1, b: 2}` evaluate to their value instead of undefined.
  // Strip a trailing semicolon so inputs like `await fetch(...);` still compile
  // as an expression.
  std::string_view expr_code =
      base::TrimWhitespaceASCII(script_code, base::TRIM_TRAILING);
  if (expr_code.ends_with(';')) {
    expr_code.remove_suffix(1);
  }
  std::string async_expr =
      base::StrCat({"(async () => (\n", expr_code, "\n))()"});
  std::string unused_expr_error;
  if (try_compile(async_expr, /*line_offset=*/1, &result.script,
                  &unused_expr_error) ||
      *terminated) {
    result.line_offset = 1;
    return result;
  }

  // Fall back to an async arrow body so bare `return` and multi-statement
  // `await` scripts work without an explicit wrapper.
  std::string async_block =
      base::StrCat({"(async () => {\n", script_code, "\n})()"});
  std::string block_error;
  if (try_compile(async_block, /*line_offset=*/1, &result.script,
                  &block_error) ||
      *terminated) {
    result.line_offset = 1;
    return result;
  }

  // If raw compilation only rejected top-level `return` or `await`, report the
  // async-block error which points to the actual syntax issue in the body.
  // Otherwise report the raw error so the synthetic wrapper isn't exposed.
  if (raw_error.find("Illegal return statement") != std::string::npos ||
      raw_error.find("await is only valid") != std::string::npos) {
    result.error_message = std::move(block_error);
  } else {
    result.error_message = std::move(raw_error);
  }
  return result;
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
  return ToUtf8(isolate, text, kExecuteJsMaxErrorMessageBytes).text;
}

mojom::ActionResultPtr MakeFailureResult(mojom::ActionResultCode code,
                                         std::string_view message,
                                         std::string console_logs = {}) {
  mojom::ActionResultPtr result =
      MakeResult(code, /*requires_page_stabilization=*/false, message);
  if (!console_logs.empty()) {
    result->execute_js_response = mojom::ExecuteJsResponse::New(
        /*result=*/std::string(), /*truncated=*/false,
        /*js_error_message=*/std::nullopt, std::move(console_logs));
  }
  return result;
}

mojom::ActionResultPtr MakeTimeoutResult(std::string_view message,
                                         std::string console_logs = {}) {
  return MakeFailureResult(mojom::ActionResultCode::kExecuteJsTimeout, message,
                           std::move(console_logs));
}

mojom::ActionResultPtr MakeScriptFailedResult(std::string_view message,
                                              std::string console_logs = {}) {
  return MakeFailureResult(mojom::ActionResultCode::kExecuteJsScriptFailed,
                           message, std::move(console_logs));
}

// Returns the result for a script that failed with a JavaScript error.
// `description` is used as both the failure message and `js_error_message`.
mojom::ActionResultPtr MakeJsErrorResult(std::string description,
                                         std::string console_logs = {}) {
  mojom::ActionResultPtr result = MakeScriptFailedResult(description);
  result->execute_js_response = mojom::ExecuteJsResponse::New(
      /*result=*/std::string(), /*truncated=*/false, std::move(description),
      std::move(console_logs));
  return result;
}

// Returns the result for a script whose promise was rejected with `error`. The
// description of `error` is both the failure message and `js_error_message`.
mojom::ActionResultPtr MakeJsErrorResult(v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::Local<v8::Value> error,
                                         base::TimeTicks deadline,
                                         std::string console_logs = {}) {
  return MakeJsErrorResult(DescribeError(isolate, context, error, deadline),
                           std::move(console_logs));
}

// Returns the result for a script that produced `value`.
mojom::ActionResultPtr MakeValueResult(v8::Isolate* isolate,
                                       v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value,
                                       base::TimeTicks deadline,
                                       std::string console_logs = {}) {
  std::optional<ScriptValueText> text =
      ConvertResult(isolate, context, value, deadline);
  if (!text) {
    return MakeTimeoutResult("Timed out converting the script result.",
                             std::move(console_logs));
  }
  mojom::ActionResultPtr result =
      MakeOkResult(/*requires_page_stabilization=*/false);
  result->execute_js_response = mojom::ExecuteJsResponse::New(
      std::move(text->text), text->truncated,
      /*js_error_message=*/std::nullopt, std::move(console_logs));
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
  std::string console_logs =
      TakeConsoleLogs(std::move(pending_promise->console_capture));
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  mojom::ActionResultPtr result =
      fulfilled
          ? MakeValueResult(isolate, context, info[0],
                            pending_promise->deadline, std::move(console_logs))
          : MakeJsErrorResult(isolate, context, info[0],
                              pending_promise->deadline,
                              std::move(console_logs));
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
  Cancel();
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
  const std::string script_code = action_->script_code;
  base::WeakPtr<ExecuteJsTool> weak_this = weak_ptr_factory_.GetWeakPtr();

  v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      web_frame->GetScriptContextFromWorldId(isolate, ISOLATED_WORLD_ID_ACTOR);
  if (context.IsEmpty()) {
    std::move(callback).Run(
        MakeScriptFailedResult("The script context is no longer available."));
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> value;
  std::string compile_error;
  int line_offset = 0;
  bool timed_out = false;
  // Declared outside `watchdog` so `watchdog.Stop()` always clears termination
  // before `active_watchdog_guard` completes any deferred console restoration.
  std::optional<ConsoleCapture::ScopedActiveWatchdog> active_watchdog_guard;
  {
    // Also bounds the microtasks the script queues, since they run before
    // script execution returns. Later continuations, e.g. after a timer or a
    // fetch, run as regular page tasks.
    ScopedTimeoutWatchdog watchdog(isolate, timeout);
    CompiledScript compiled =
        CompileScript(isolate, context, script_code, &timed_out);
    line_offset = compiled.line_offset;
    if (!timed_out && !compiled.script.IsEmpty()) {
      console_capture_ = ConsoleCapture::Install(isolate, context, deadline);
      active_watchdog_guard.emplace(console_capture_.get());
      if (!try_catch.HasTerminated()) {
        v8::MicrotasksScope microtasks_scope(
            context, v8::MicrotasksScope::kRunMicrotasks);
        std::ignore = compiled.script->Run(context).ToLocal(&value);
      }
    } else if (!timed_out) {
      compile_error = std::move(compiled.error_message);
    }
    timed_out = watchdog.Stop() || timed_out || try_catch.HasTerminated();
  }
  active_watchdog_guard.reset();

  if (!weak_this) {
    // `callback` is bound to a WeakPtr to the ToolExecutor, which was
    // invalidated when this tool was cancelled or the frame went away.
    // `active_watchdog_guard` above has already completed any deferred
    // `console.*` restoration if termination raced with Cancel().
    return;
  }

  if (timed_out) {
    std::move(callback).Run(MakeTimeoutResult(
        "The script timed out.", TakeConsoleLogs(std::move(console_capture_))));
    return;
  }

  if (!compile_error.empty()) {
    std::move(callback).Run(MakeJsErrorResult(std::move(compile_error)));
    return;
  }

  if (try_catch.HasCaught()) {
    std::optional<std::string> description =
        FormatTryCatchMessage(isolate, context, try_catch, line_offset);
    std::string console_logs = TakeConsoleLogs(std::move(console_capture_));
    if (description) {
      std::move(callback).Run(
          MakeJsErrorResult(std::move(*description), std::move(console_logs)));
    } else {
      std::move(callback).Run(MakeJsErrorResult(isolate, context,
                                                try_catch.Exception(), deadline,
                                                std::move(console_logs)));
    }
    return;
  }

  if (value.IsEmpty()) {
    std::move(callback).Run(MakeScriptFailedResult(
        "The script threw an exception or has a syntax error.",
        TakeConsoleLogs(std::move(console_capture_))));
    return;
  }

  if (value->IsPromise()) {
    v8::Local<v8::Promise> promise = value.As<v8::Promise>();
    switch (promise->State()) {
      case v8::Promise::kFulfilled:
        value = promise->Result();
        break;
      case v8::Promise::kRejected:
        promise->MarkAsHandled();
        std::move(callback).Run(
            MakeJsErrorResult(isolate, context, promise->Result(), deadline,
                              TakeConsoleLogs(std::move(console_capture_))));
        return;
      case v8::Promise::kPending: {
        const uint32_t id = AddPendingPromise(
            {std::move(callback), deadline, std::move(console_capture_)});
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
          std::string console_logs =
              TakeConsoleLogs(std::move(pending_promise->console_capture));
          std::move(pending_promise->callback)
              .Run(MakeScriptFailedResult(
                  "Failed to wait for the promise the script returned.",
                  std::move(console_logs)));
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

  std::move(callback).Run(
      MakeValueResult(isolate, context, value, deadline,
                      TakeConsoleLogs(std::move(console_capture_))));
}

void ExecuteJsTool::Cancel() {
  console_capture_.reset();
  if (pending_promise_id_) {
    std::ignore = TakePendingPromise(std::exchange(pending_promise_id_, 0));
  }
  promise_timeout_timer_.Stop();
}

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
  std::string console_logs =
      TakeConsoleLogs(std::move(pending_promise->console_capture));
  std::move(pending_promise->callback)
      .Run(MakeTimeoutResult(
          "Timed out waiting for the promise the script returned.",
          std::move(console_logs)));
}

}  // namespace actor
