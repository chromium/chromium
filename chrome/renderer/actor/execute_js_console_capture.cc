// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_console_capture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/renderer/actor/execute_js_v8_utils.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

struct ConsoleMethodInfo {
  const char* name;
  std::string_view prefix;
};

constexpr std::array<ConsoleMethodInfo, 4> kConsoleMethods = {{
    {"log", "[log]"},
    {"info", "[info]"},
    {"warn", "[warn]"},
    {"error", "[error]"},
}};

}  // namespace

// Captured output for an active ConsoleCapture. Ref-counted so that a nested
// run loop destroying ConsoleCapture while formatting an argument cannot
// invalidate the active callback's reference, and so deferred restoration of
// `console.*` can still complete if async watchdog termination raced with
// ConsoleCapture teardown.
struct ConsoleCaptureState : public base::RefCounted<ConsoleCaptureState> {
  struct WrappedMethod {
    bool wrapped = false;
    bool had_own_property = false;
    v8::PropertyAttribute attributes = v8::None;
    v8::Global<v8::Value> original;
    v8::Global<v8::Function> wrapper;
  };

  ConsoleCaptureState(v8::Isolate* isolate,
                      v8::Local<v8::Context> local_context,
                      v8::Local<v8::Object> console_obj,
                      base::TimeTicks deadline)
      : deadline(deadline),
        context(isolate, local_context),
        console(isolate, console_obj) {}

  void ResetRestoreHandles() {
    methods_restored = true;
    for (auto& entry : entries) {
      entry.original.Reset();
      entry.wrapper.Reset();
      entry.wrapped = false;
    }
    console.Reset();
    context.Reset();
  }

  void TryRestoreConsoleMethods(v8::Isolate* isolate) {
    if (methods_restored) {
      return;
    }
    if (isolate->IsExecutionTerminating()) {
      // If `OnConsoleMethodCalled` or an outer `ScopedActiveWatchdog` is
      // currently on the stack, defer restoring until the watchdog cancels
      // termination (or clears handles if an external termination propagates).
      // Otherwise release handles now.
      if (!in_capture && !has_active_watchdog) {
        ResetRestoreHandles();
      }
      return;
    }
    if (context.IsEmpty() || console.IsEmpty()) {
      ResetRestoreHandles();
      return;
    }

    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> local_context = context.Get(isolate);
    v8::Local<v8::Object> console_obj = console.Get(isolate);
    v8::Context::Scope context_scope(local_context);
    v8::MicrotasksScope microtasks_scope(
        isolate, local_context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::TryCatch try_catch(isolate);

    for (size_t i = 0; i < kConsoleMethods.size(); ++i) {
      const WrappedMethod& entry = entries[i];
      if (!entry.wrapped) {
        continue;
      }
      v8::Local<v8::String> method_key;
      if (!v8::String::NewFromUtf8(isolate, kConsoleMethods[i].name,
                                   v8::NewStringType::kNormal)
               .ToLocal(&method_key)) {
        try_catch.Reset();
        continue;
      }
      bool has_own = false;
      if (!console_obj->HasOwnProperty(local_context, method_key)
               .To(&has_own) ||
          !has_own) {
        try_catch.Reset();
        continue;
      }
      v8::Local<v8::Value> current;
      if (!console_obj->TryGetWithNoSideEffects(local_context, method_key)
               .ToLocal(&current) ||
          !current->StrictEquals(entry.wrapper.Get(isolate))) {
        try_catch.Reset();
        continue;
      }
      bool method_restored = false;
      if (entry.had_own_property) {
        method_restored = console_obj
                              ->DefineOwnProperty(local_context, method_key,
                                                  entry.original.Get(isolate),
                                                  entry.attributes)
                              .FromMaybe(false);
      } else {
        method_restored =
            console_obj->Delete(local_context, method_key).FromMaybe(false);
      }
      if (!method_restored) {
        try_catch.Reset();
        // If the script sealed `console` or changed attributes (e.g.
        // `enumerable: false, configurable: false`) while keeping it writable,
        // retry with its current attributes so the wrapper is still replaced
        // with the original function.
        v8::PropertyAttribute current_attrs = v8::None;
        bool has_attrs = false;
        if (console_obj
                ->GetPropertyAttributes(local_context, method_key,
                                        &current_attrs)
                .To(&has_attrs) &&
            has_attrs) {
          std::ignore = console_obj->DefineOwnProperty(
              local_context, method_key, entry.original.Get(isolate),
              current_attrs);
        }
      }
      try_catch.Reset();
    }

    ResetRestoreHandles();
  }

  std::string buffer;
  size_t line_count = 0;
  base::TimeTicks deadline;
  bool capped = false;
  bool in_capture = false;
  bool has_active_watchdog = false;
  bool restored = false;
  bool methods_restored = false;
  // Lazily compiled normalizer function, reused across `console.*` calls in
  // this capture and cleared on Restore().
  v8::Global<v8::Function> normalizer_fn;
  v8::Global<v8::Context> context;
  v8::Global<v8::Object> console;
  std::array<WrappedMethod, kConsoleMethods.size()> entries;

 private:
  friend class base::RefCounted<ConsoleCaptureState>;
  ~ConsoleCaptureState() = default;
};

namespace {

// Only accessed on the render main thread.
base::flat_map<uint32_t, scoped_refptr<ConsoleCaptureState>>&
GetActiveConsoleCaptures() {
  static base::NoDestructor<
      base::flat_map<uint32_t, scoped_refptr<ConsoleCaptureState>>>
      captures;
  return *captures;
}

uint32_t AddActiveConsoleCapture(scoped_refptr<ConsoleCaptureState> state) {
  static uint32_t next_id = 0;
  if (++next_id == 0) {
    ++next_id;
  }
  GetActiveConsoleCaptures().emplace(next_id, std::move(state));
  return next_id;
}

scoped_refptr<ConsoleCaptureState> LookupActiveConsoleCapture(uint32_t id) {
  const auto& captures = GetActiveConsoleCaptures();
  auto it = captures.find(id);
  return it == captures.end() ? nullptr : it->second;
}

// Formats a single `console.*` argument into at most `max_bytes` UTF-8 bytes.
// Caller must already have entered `context`. Routes object values through
// `NormalizeResultValue` so DOM Elements, NodeLists, HTMLCollections, Maps,
// Sets, and circular objects format consistently with script return values
// instead of "{}". Sets `*truncated` if the argument was cut to fit
// `max_bytes`, and sets `*terminated` (and rethrows) if V8 execution is
// terminated while stringifying `value`.
std::string FormatConsoleArg(v8::Isolate* isolate,
                             v8::Local<v8::Context> context,
                             v8::Local<v8::Value> value,
                             size_t max_bytes,
                             v8::Local<v8::Function>* normalizer_fn,
                             bool* truncated,
                             bool* terminated) {
  auto truncate_literal = [&](std::string_view text) -> std::string {
    std::string_view result = base::TruncateUTF8ToByteSize(text, max_bytes);
    *truncated = result.size() < text.size();
    return std::string(result);
  };
  if (max_bytes == 0) {
    *truncated = true;
    return {};
  }
  if (value->IsString()) {
    ScriptValueText utf8 = ToUtf8(isolate, value.As<v8::String>(), max_bytes);
    *truncated = utf8.truncated;
    return std::move(utf8.text);
  }
  if (value->IsUndefined()) {
    return truncate_literal("undefined");
  }
  if (value->IsNull()) {
    return truncate_literal("null");
  }
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> text;
  if (value->IsSymbol()) {
    if (value->ToDetailString(context).ToLocal(&text)) {
      ScriptValueText utf8 = ToUtf8(isolate, text, max_bytes);
      *truncated = utf8.truncated;
      return std::move(utf8.text);
    }
    if (try_catch.HasTerminated()) {
      *terminated = true;
      try_catch.ReThrow();
      return {};
    }
    return truncate_literal("Symbol()");
  }
  if (value->IsObject() && !value->IsFunction() && !value->IsNativeError() &&
      !value->IsRegExp()) {
    v8::Local<v8::Value> normalized;
    if (NormalizeResultValue(isolate, context, value, normalizer_fn,
                             /*for_console_arg=*/true)
            .ToLocal(&normalized)) {
      if (v8::JSON::Stringify(context, normalized).ToLocal(&text)) {
        ScriptValueText utf8 = ToUtf8(isolate, text, max_bytes);
        *truncated = utf8.truncated;
        return std::move(utf8.text);
      }
      if (try_catch.HasTerminated()) {
        *terminated = true;
        try_catch.ReThrow();
        return {};
      }
      try_catch.Reset();
    } else {
      if (try_catch.HasTerminated()) {
        *terminated = true;
        try_catch.ReThrow();
        return {};
      }
      try_catch.Reset();
    }
  }
  if (value->ToString(context).ToLocal(&text)) {
    ScriptValueText utf8 = ToUtf8(isolate, text, max_bytes);
    *truncated = utf8.truncated;
    return std::move(utf8.text);
  }
  if (try_catch.HasTerminated()) {
    *terminated = true;
    try_catch.ReThrow();
    return {};
  }
  return truncate_literal("[Unserializable]");
}

struct FormattedConsoleLine {
  std::string line;
  size_t line_breaks = 0;
  bool line_capped = false;
  bool arg_truncated = false;
  bool drop_line = false;
  bool terminated = false;
};

bool ClampLineBreaks(std::string& line,
                     size_t start_pos,
                     size_t remaining_lines,
                     size_t& line_breaks) {
  for (size_t pos = start_pos;
       (pos = line.find('\n', pos)) != std::string::npos; ++pos) {
    if (++line_breaks == remaining_lines) {
      line.resize(pos);
      return true;
    }
  }
  return false;
}

// Lazily arms an async formatting watchdog on the first argument whose
// conversion can run user JS or unbounded formatting, avoiding a delayed
// ThreadPool task for 0-arg or primitive-only calls and avoiding an overlapping
// watchdog when another invocation already guards `isolate`. Returns false if
// the capture deadline has already elapsed.
bool EnsureAsyncWatchdog(v8::Isolate* isolate,
                         const ConsoleCaptureState& state,
                         std::optional<v8::TryCatch>& async_try_catch,
                         std::optional<ScopedTimeoutWatchdog>& async_watchdog) {
  if (state.has_active_watchdog || async_watchdog ||
      ScopedTimeoutWatchdog::HasActiveWatchdog(isolate)) {
    return true;
  }
  const base::TimeDelta remaining = state.deadline - base::TimeTicks::Now();
  if (!remaining.is_positive()) {
    return false;
  }
  async_try_catch.emplace(isolate);
  async_watchdog.emplace(isolate, remaining);
  return true;
}

FormattedConsoleLine FormatConsoleLine(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const v8::FunctionCallbackInfo<v8::Value>& info,
    std::string_view prefix,
    size_t max_line_bytes,
    size_t remaining_lines,
    ConsoleCaptureState& state,
    v8::Local<v8::Function>* normalizer_fn,
    std::optional<v8::TryCatch>& async_try_catch,
    std::optional<ScopedTimeoutWatchdog>& async_watchdog) {
  FormattedConsoleLine formatted;
  formatted.line = std::string(prefix);
  for (int i = 0; i < info.Length(); ++i) {
    if (formatted.line.size() + 1 >= max_line_bytes) {
      formatted.arg_truncated = true;
      break;
    }
    if ((info[i]->IsObject() || info[i]->IsBigInt()) &&
        !EnsureAsyncWatchdog(isolate, state, async_try_catch, async_watchdog)) {
      state.capped = true;
      formatted.drop_line = true;
      break;
    }
    const size_t arg_budget = max_line_bytes - formatted.line.size() - 1;
    bool arg_was_truncated = false;
    std::string arg =
        FormatConsoleArg(isolate, context, info[i], arg_budget, normalizer_fn,
                         &arg_was_truncated, &formatted.terminated);
    if (formatted.terminated || state.restored) {
      break;
    }
    if (arg.empty() && arg_was_truncated) {
      formatted.arg_truncated = true;
      formatted.drop_line = (i == 0);
      break;
    }
    const size_t arg_start = formatted.line.size() + 1;
    formatted.line.push_back(' ');
    formatted.line.append(arg);
    formatted.line_capped = ClampLineBreaks(
        formatted.line, arg_start, remaining_lines, formatted.line_breaks);
    if (arg_was_truncated || formatted.line_capped) {
      formatted.arg_truncated = true;
      break;
    }
  }
  return formatted;
}

// Captures a single `console.*` invocation into the active ConsoleCaptureState
// identified by `capture_id`, if active and under caps. Returns false if V8
// execution is terminating and the caller must abort without forwarding to the
// original console method.
bool CaptureConsoleMessage(v8::Isolate* isolate,
                           v8::Local<v8::Context> context,
                           const v8::FunctionCallbackInfo<v8::Value>& info,
                           uint32_t capture_id,
                           uint32_t method_index) {
  if (isolate->IsExecutionTerminating()) {
    return false;
  }
  scoped_refptr<ConsoleCaptureState> state =
      LookupActiveConsoleCapture(capture_id);
  if (!state || state->in_capture || state->capped ||
      state->line_count >= kExecuteJsMaxConsoleLines ||
      state->buffer.size() >= kExecuteJsMaxConsoleBytes) {
    return true;
  }

  base::AutoReset<bool> in_capture_guard(&state->in_capture, true);
  const size_t separator_bytes = state->buffer.empty() ? 0 : 1;
  const std::string_view prefix = kConsoleMethods[method_index].prefix;
  const size_t min_needed =
      info.Length() > 0 ? prefix.size() + 2 : prefix.size();
  if (state->buffer.size() + separator_bytes + min_needed >
      kExecuteJsMaxConsoleBytes) {
    state->capped = true;
    return true;
  }
  if (!state->has_active_watchdog &&
      base::TimeTicks::Now() >= state->deadline) {
    state->capped = true;
    return true;
  }

  const size_t max_line_bytes =
      kExecuteJsMaxConsoleBytes - state->buffer.size() - separator_bytes;
  const size_t remaining_lines = kExecuteJsMaxConsoleLines - state->line_count;
  v8::Local<v8::Function> normalizer_fn;
  if (!state->normalizer_fn.IsEmpty()) {
    normalizer_fn = state->normalizer_fn.Get(isolate);
  }

  std::optional<v8::TryCatch> async_try_catch;
  std::optional<ScopedTimeoutWatchdog> async_watchdog;
  FormattedConsoleLine formatted = FormatConsoleLine(
      isolate, context, info, prefix, max_line_bytes, remaining_lines, *state,
      &normalizer_fn, async_try_catch, async_watchdog);

  if (async_watchdog) {
    const bool timed_out_by_async_watchdog = async_watchdog->Stop();
    if (formatted.terminated || async_try_catch->HasTerminated() ||
        timed_out_by_async_watchdog) {
      if (timed_out_by_async_watchdog && !isolate->IsExecutionTerminating()) {
        // Intentionally swallow our async formatting timeout rather than
        // aborting unrelated page JS over a getter that native console.* does
        // not evaluate; marking `capped = true` prevents further formatting
        // work for this invocation.
        async_try_catch->Reset();
        state->capped = true;
        formatted.drop_line = true;
      } else {
        if (state->restored) {
          state->ResetRestoreHandles();
        }
        async_try_catch->ReThrow();
        return false;
      }
    }
  } else if (formatted.terminated) {
    if (!state->has_active_watchdog && state->restored) {
      state->ResetRestoreHandles();
    }
    return false;
  }

  if (state->restored) {
    if (isolate->IsExecutionTerminating()) {
      if (!state->has_active_watchdog) {
        state->ResetRestoreHandles();
      }
      return false;
    }
    state->TryRestoreConsoleMethods(isolate);
    return true;
  }

  if (!formatted.drop_line) {
    if (!state->buffer.empty()) {
      state->buffer.push_back('\n');
    }
    state->buffer.append(formatted.line);
    state->line_count +=
        formatted.line_capped ? remaining_lines : formatted.line_breaks + 1;
  }
  if (formatted.arg_truncated ||
      state->buffer.size() >= kExecuteJsMaxConsoleBytes ||
      state->line_count >= kExecuteJsMaxConsoleLines) {
    state->capped = true;
  }
  if (state->normalizer_fn.IsEmpty() && !normalizer_fn.IsEmpty() &&
      !state->capped) {
    state->normalizer_fn.Reset(isolate, normalizer_fn);
  }
  return true;
}

void ForwardConsoleCall(v8::Isolate* isolate,
                        v8::Local<v8::Context> context,
                        const v8::FunctionCallbackInfo<v8::Value>& info,
                        v8::Local<v8::Function> orig_fn) {
  v8::LocalVector<v8::Value> args(isolate);
  args.reserve(info.Length());
  for (int i = 0; i < info.Length(); ++i) {
    args.push_back(info[i]);
  }
  v8::Local<v8::Value> call_result;
  if (orig_fn
          ->Call(context, info.This(), static_cast<int>(args.size()),
                 args.empty() ? nullptr : args.data())
          .ToLocal(&call_result)) {
    info.GetReturnValue().Set(call_result);
  }
}

void OnConsoleMethodCalled(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> data = info.DataV2().As<v8::Value>().As<v8::Array>();

  v8::Local<v8::Value> id_val;
  v8::Local<v8::Value> method_val;
  v8::Local<v8::Value> orig_val;
  if (!data->Get(context, 0).ToLocal(&id_val) || !id_val->IsUint32() ||
      !data->Get(context, 1).ToLocal(&method_val) || !method_val->IsUint32() ||
      !data->Get(context, 2).ToLocal(&orig_val) || !orig_val->IsFunction()) {
    return;
  }

  const uint32_t capture_id = id_val.As<v8::Uint32>()->Value();
  const uint32_t method_index = method_val.As<v8::Uint32>()->Value();
  if (method_index >= kConsoleMethods.size()) {
    return;
  }

  if (!CaptureConsoleMessage(isolate, context, info, capture_id,
                             method_index) ||
      isolate->IsExecutionTerminating()) {
    return;
  }

  ForwardConsoleCall(isolate, context, info, orig_val.As<v8::Function>());
}

}  // namespace

ConsoleCapture::ScopedActiveWatchdog::ScopedActiveWatchdog(
    ConsoleCapture* capture)
    : isolate_(capture ? capture->isolate_ : nullptr),
      state_(capture ? capture->state_ : nullptr) {
  if (state_) {
    previous_value_ = std::exchange(state_->has_active_watchdog, true);
  }
}

ConsoleCapture::ScopedActiveWatchdog::~ScopedActiveWatchdog() {
  if (state_) {
    state_->has_active_watchdog = previous_value_;
    if (state_->restored) {
      state_->TryRestoreConsoleMethods(isolate_);
    }
  }
}

// static
std::unique_ptr<ConsoleCapture> ConsoleCapture::Install(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    base::TimeTicks deadline) {
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> console_key =
      v8::String::NewFromUtf8Literal(isolate, "console");
  v8::Local<v8::Value> console_val;
  if (!context->Global()
           ->TryGetWithNoSideEffects(context, console_key)
           .ToLocal(&console_val) ||
      !console_val->IsObject() || console_val->IsProxy()) {
    if (try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return nullptr;
  }
  v8::Local<v8::Object> console_obj = console_val.As<v8::Object>();

  auto state = base::MakeRefCounted<ConsoleCaptureState>(isolate, context,
                                                         console_obj, deadline);
  const uint32_t capture_id = AddActiveConsoleCapture(state);
  auto capture =
      base::WrapUnique(new ConsoleCapture(isolate, capture_id, state));

  size_t wrapped_count = 0;
  for (uint32_t i = 0; i < kConsoleMethods.size(); ++i) {
    v8::Local<v8::String> method_key;
    if (!v8::String::NewFromUtf8(isolate, kConsoleMethods[i].name,
                                 v8::NewStringType::kNormal)
             .ToLocal(&method_key)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }
    v8::Local<v8::Value> orig_val;
    if (!console_obj->TryGetWithNoSideEffects(context, method_key)
             .ToLocal(&orig_val)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }
    if (!orig_val->IsFunction()) {
      continue;
    }
    bool had_own = false;
    if (!console_obj->HasOwnProperty(context, method_key).To(&had_own)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }
    v8::PropertyAttribute attrs = v8::None;
    if (had_own) {
      bool has_attrs = false;
      if (!console_obj->GetPropertyAttributes(context, method_key, &attrs)
               .To(&has_attrs) ||
          !has_attrs) {
        if (try_catch.HasTerminated()) {
          try_catch.ReThrow();
          break;
        }
        try_catch.Reset();
        continue;
      }
      if ((attrs & v8::ReadOnly) && (attrs & v8::DontDelete)) {
        continue;
      }
    }

    v8::Local<v8::Array> data = v8::Array::New(isolate, 3);
    if (!data->CreateDataProperty(
                 context, 0, v8::Integer::NewFromUnsigned(isolate, capture_id))
             .FromMaybe(false) ||
        !data->CreateDataProperty(context, 1,
                                  v8::Integer::NewFromUnsigned(isolate, i))
             .FromMaybe(false) ||
        !data->CreateDataProperty(context, 2, orig_val).FromMaybe(false)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }

    v8::Local<v8::Function> wrapper;
    if (!v8::Function::New(context, OnConsoleMethodCalled, data,
                           /*length=*/0, v8::ConstructorBehavior::kThrow)
             .ToLocal(&wrapper)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }

    const v8::PropertyAttribute install_attrs = had_own ? attrs : v8::None;
    if (!console_obj
             ->DefineOwnProperty(context, method_key, wrapper, install_attrs)
             .FromMaybe(false)) {
      if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        break;
      }
      try_catch.Reset();
      continue;
    }

    state->entries[i].wrapped = true;
    state->entries[i].had_own_property = had_own;
    state->entries[i].attributes = install_attrs;
    state->entries[i].original.Reset(isolate, orig_val);
    state->entries[i].wrapper.Reset(isolate, wrapper);
    ++wrapped_count;
  }

  if (wrapped_count == 0) {
    capture->Restore();
    return nullptr;
  }
  return capture;
}

ConsoleCapture::ConsoleCapture(v8::Isolate* isolate,
                               uint32_t capture_id,
                               scoped_refptr<ConsoleCaptureState> state)
    : isolate_(isolate), capture_id_(capture_id), state_(std::move(state)) {}

ConsoleCapture::~ConsoleCapture() {
  Restore();
}

std::string ConsoleCapture::TakeLogsAndRestore() {
  Restore();
  if (!state_ || state_->buffer.empty()) {
    return {};
  }
  return std::exchange(state_->buffer, {});
}

void ConsoleCapture::Restore() {
  if (restored_) {
    return;
  }
  restored_ = true;
  GetActiveConsoleCaptures().erase(capture_id_);
  if (state_) {
    state_->restored = true;
    state_->normalizer_fn.Reset();
    state_->TryRestoreConsoleMethods(isolate_);
  }
}

std::string TakeConsoleLogs(std::unique_ptr<ConsoleCapture> capture) {
  if (!capture) {
    return {};
  }
  return capture->TakeLogsAndRestore();
}

}  // namespace actor
