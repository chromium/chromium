// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_tool.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
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

// Maximum size, in bytes, of the message describing a script failure.
constexpr size_t kMaxErrorMessageBytes = 1024;

// Normalizes values that JSON.stringify() would otherwise serialize to "{}"
// (DOM Elements, NodeLists, HTMLCollections, Errors, Maps, Sets, RegExps)
// before stringifying. Runs inside the actor's isolated world without touching
// globals.
constexpr char kResultNormalizerScript[] = R"js(
(function(root) {
  const kMaxCollectionItems = 100;
  const kMaxTextLength = 200;
  const kMaxDepth = 32;
  const kMaxNodes = 20000;
  const seen = new WeakSet();
  let nodeCount = 0;

  function getGetter(proto, prop) {
    return proto
        ? Object.getOwnPropertyDescriptor(proto, prop)?.get
        : undefined;
  }

  const elementProto =
      typeof Element !== 'undefined' ? Element.prototype : null;
  const htmlElementProto =
      typeof HTMLElement !== 'undefined' ? HTMLElement.prototype : null;
  const htmlInputProto = typeof HTMLInputElement !== 'undefined'
      ? HTMLInputElement.prototype
      : null;
  const nodeProto = typeof Node !== 'undefined' ? Node.prototype : null;
  const nodeListProto =
      typeof NodeList !== 'undefined' ? NodeList.prototype : null;
  const htmlCollectionProto = typeof HTMLCollection !== 'undefined'
      ? HTMLCollection.prototype
      : null;

  const elTagName = getGetter(elementProto, 'tagName');
  const elId = getGetter(elementProto, 'id');
  const elRole = getGetter(elementProto, 'role');
  const elAriaLabel = getGetter(elementProto, 'ariaLabel');
  const elClientLeft = getGetter(elementProto, 'clientLeft');
  const elClientTop = getGetter(elementProto, 'clientTop');
  const elGetAttribute = elementProto?.getAttribute;
  const elHasAttribute = elementProto?.hasAttribute;
  const elGetBoundingClientRect = elementProto?.getBoundingClientRect;
  const htmlElInnerText = getGetter(htmlElementProto, 'innerText');
  const htmlInputType = getGetter(htmlInputProto, 'type');
  const nodeTextContent = getGetter(nodeProto, 'textContent');
  const nodeOwnerDocument = getGetter(nodeProto, 'ownerDocument');
  const nodeListLength = getGetter(nodeListProto, 'length');
  const nodeListItem = nodeListProto?.item;
  const htmlCollectionLength = getGetter(htmlCollectionProto, 'length');
  const htmlCollectionItem = htmlCollectionProto?.item;
  const regexpSource = getGetter(RegExp.prototype, 'source');

  function isElement(v) {
    if (!elTagName) {
      return false;
    }
    try {
      elTagName.call(v);
      return true;
    } catch (e) {
      return false;
    }
  }

  function serializeElement(el) {
    const out = { tag: elTagName.call(el).toLowerCase() };
    const id = elId ? elId.call(el) : '';
    if (id) {
      out.id = id;
    }
    const role = (elGetAttribute && elGetAttribute.call(el, 'role')) ||
        (elRole && elRole.call(el));
    if (role) {
      out.role = role;
    }
    const ariaLabel =
        (elGetAttribute && elGetAttribute.call(el, 'aria-label')) ||
        (elAriaLabel && elAriaLabel.call(el));
    if (ariaLabel) {
      out.ariaLabel = ariaLabel;
    }
    const rect = elGetBoundingClientRect.call(el);
    let rawText = '';
    let hasInnerText = false;
    if (htmlElInnerText && (rect.width > 0 || rect.height > 0)) {
      try {
        const inner = htmlElInnerText.call(el);
        if (typeof inner === 'string') {
          rawText = inner;
          hasInnerText = true;
        }
      } catch (e) {
      }
    }
    if (!hasInnerText && nodeTextContent) {
      rawText = nodeTextContent.call(el) || '';
    }
    const text = rawText.trimStart()
                     .slice(0, kMaxTextLength * 4)
                     .replace(/\s+/g, ' ')
                     .trim()
                     .slice(0, kMaxTextLength);
    if (text) {
      out.text = text;
    }
    let href = null;
    try {
      href = typeof el.href === 'string'
          ? el.href
          : elGetAttribute.call(el, 'href');
    } catch (e) {
      href = elGetAttribute.call(el, 'href');
    }
    if (href) {
      out.href = href;
    }
    let inputType = '';
    if (htmlInputType) {
      try {
        inputType = String(htmlInputType.call(el) || '').toLowerCase();
      } catch (e) {
      }
    }
    const omitValue =
        inputType === 'password' ||
        ((inputType === 'checkbox' || inputType === 'radio') &&
         !elHasAttribute.call(el, 'value'));
    try {
      if (!omitValue && typeof el.value === 'string' && el.value !== '') {
        out.value = el.value.slice(0, kMaxTextLength);
      }
    } catch (e) {
    }
    let left = rect.left;
    let top = rect.top;
    let win = null;
    try {
      const doc = nodeOwnerDocument ? nodeOwnerDocument.call(el) : null;
      win = doc && doc.defaultView;
      while (win && typeof window !== 'undefined' && win !== window &&
             win !== win.parent && win.frameElement) {
        const frameEl = win.frameElement;
        const frameRect = elGetBoundingClientRect.call(frameEl);
        const parentWin = win.parent;
        const frameStyle = parentWin.getComputedStyle(frameEl);
        left += frameRect.left + elClientLeft.call(frameEl) +
                (parseFloat(frameStyle.paddingLeft) || 0);
        top += frameRect.top + elClientTop.call(frameEl) +
               (parseFloat(frameStyle.paddingTop) || 0);
        win = parentWin;
      }
    } catch (e) {
      win = null;
    }
    if (typeof window !== 'undefined' && win === window &&
        rect.width > 0 && rect.height > 0) {
      const vw = window.innerWidth;
      const vh = window.innerHeight;
      if (vw > 0 && vh > 0) {
        out.center_0_999 = {
          x: Math.round(((left + rect.width / 2) / vw) * 999),
          y: Math.round(((top + rect.height / 2) / vh) * 999),
        };
      }
    }
    return out;
  }

  function unboxPrimitive(v, tag) {
    try {
      if (tag === '[object Number]') {
        return Number.prototype.valueOf.call(v);
      }
      if (tag === '[object String]') {
        return String.prototype.valueOf.call(v);
      }
      if (tag === '[object Boolean]') {
        return Boolean.prototype.valueOf.call(v);
      }
      if (tag === '[object BigInt]') {
        return BigInt.prototype.valueOf.call(v).toString();
      }
    } catch (e) {
      // Spoofed Symbol.toStringTag; fall through to object traversal.
    }
    return undefined;
  }

  function normalize(v, depth) {
    if (++nodeCount > kMaxNodes) {
      return '[Truncated]';
    }
    if (v === null || v === undefined) {
      return v;
    }
    if (typeof v === 'bigint') {
      return v.toString();
    }
    if (typeof v !== 'object') {
      return v;
    }
    if (depth >= kMaxDepth) {
      return '[MaxDepth]';
    }
    if (isElement(v)) {
      try {
        return serializeElement(v);
      } catch (e) {
        return '[Unserializable]';
      }
    }
    let tag = '';
    try {
      tag = Object.prototype.toString.call(v);
    } catch (e) {
      return '[Unserializable]';
    }
    if (v instanceof Error || tag === '[object Error]' ||
        tag === '[object DOMException]') {
      let name = 'Error';
      let message = '';
      try {
        const n = v.name;
        if (typeof n === 'string' && n) {
          name = n;
        }
      } catch (e) {
      }
      try {
        const m = v.message;
        if (typeof m === 'string') {
          message = m;
        } else if (m != null) {
          message = String(m);
        }
      } catch (e) {
      }
      return { name, message };
    }
    if (v instanceof RegExp || tag === '[object RegExp]') {
      try {
        if (regexpSource) {
          regexpSource.call(v);
        }
        return RegExp.prototype.toString.call(v);
      } catch (e) {
      }
    }
    const unboxed = unboxPrimitive(v, tag);
    if (unboxed !== undefined) {
      return unboxed;
    }
    if (seen.has(v)) {
      return '[Circular]';
    }
    seen.add(v);
    try {
      let collectionLen = null;
      let collectionItem = null;
      if (nodeListLength && nodeListItem) {
        try {
          collectionLen = nodeListLength.call(v);
          collectionItem = nodeListItem;
        } catch (e) {
        }
      }
      if (collectionLen === null && htmlCollectionLength &&
          htmlCollectionItem) {
        try {
          collectionLen = htmlCollectionLength.call(v);
          collectionItem = htmlCollectionItem;
        } catch (e) {
        }
      }
      if (collectionLen !== null) {
        const len = Math.min(collectionLen, kMaxCollectionItems);
        const out = new Array(len);
        for (let i = 0; i < len; ++i) {
          if (nodeCount >= kMaxNodes) {
            out.length = i;
            out.push('[Truncated]');
            break;
          }
          try {
            out[i] = normalize(collectionItem.call(v, i), depth + 1);
          } catch (e) {
            ++nodeCount;
            out[i] = '[Unserializable]';
          }
        }
        return out;
      }
      if (v instanceof Set || tag === '[object Set]') {
        let iter = null;
        try {
          iter = Set.prototype.values.call(v);
        } catch (e) {
        }
        if (iter) {
          const out = [];
          for (const item of iter) {
            if (nodeCount >= kMaxNodes) {
              out.push('[Truncated]');
              break;
            }
            out.push(normalize(item, depth + 1));
          }
          return out;
        }
      }
      if (v instanceof Map || tag === '[object Map]') {
        let iter = null;
        try {
          iter = Map.prototype.entries.call(v);
        } catch (e) {
        }
        if (iter) {
          const out = [];
          for (const [k, val] of iter) {
            if (nodeCount >= kMaxNodes) {
              out.push('[Truncated]');
              break;
            }
            out.push([normalize(k, depth + 1), normalize(val, depth + 1)]);
          }
          return out;
        }
      }
      let toJsonFn = null;
      try {
        const fn = v.toJSON;
        if (typeof fn === 'function') {
          toJsonFn = fn;
        }
      } catch (e) {
        return '[Unserializable]';
      }
      if (toJsonFn) {
        try {
          const json = toJsonFn.call(v);
          if (json !== v) {
            return normalize(json, depth + 1);
          }
        } catch (e) {
          return '[Unserializable]';
        }
      }
      if (Array.isArray(v)) {
        let len = 0;
        try {
          len = v.length;
        } catch (e) {
          return '[Unserializable]';
        }
        const out = [];
        for (let i = 0; i < len; ++i) {
          if (nodeCount >= kMaxNodes) {
            out.push('[Truncated]');
            break;
          }
          try {
            out.push(normalize(v[i], depth + 1));
          } catch (e) {
            ++nodeCount;
            out.push('[Unserializable]');
          }
        }
        return out;
      }
      const out = Object.create(null);
      let keys;
      try {
        keys = Object.keys(v);
      } catch (e) {
        return '[Unserializable]';
      }
      for (const key of keys) {
        if (nodeCount >= kMaxNodes) {
          out[key] = '[Truncated]';
          break;
        }
        try {
          out[key] = normalize(v[key], depth + 1);
        } catch (e) {
          ++nodeCount;
          out[key] = '[Unserializable]';
        }
      }
      return out;
    } finally {
      seen.delete(v);
    }
  }

  return normalize(root, 0);
})
)js";

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

// Runs `kResultNormalizerScript` in `context` on `value`. Caller must hold a
// v8::Context::Scope, v8::MicrotasksScope, v8::TryCatch, and
// ScopedTimeoutWatchdog.
v8::MaybeLocal<v8::Value> NormalizeResultValue(v8::Isolate* isolate,
                                               v8::Local<v8::Context> context,
                                               v8::Local<v8::Value> value) {
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8Literal(isolate, kResultNormalizerScript);
  v8::Local<v8::Script> script;
  v8::Local<v8::Value> fn_value;
  if (!v8::Script::Compile(context, source).ToLocal(&script) ||
      !script->Run(context).ToLocal(&fn_value) || !fn_value->IsFunction()) {
    return {};
  }
  v8::Local<v8::Value> argv[] = {value};
  return fn_value.As<v8::Function>()->Call(context, v8::Undefined(isolate), 1,
                                           argv);
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
  const size_t max_message_bytes = kMaxErrorMessageBytes > suffix.size()
                                       ? kMaxErrorMessageBytes - suffix.size()
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

// Returns the result for a script that failed with a JavaScript error.
// `description` is used as both the failure message and `js_error_message`.
mojom::ActionResultPtr MakeJsErrorResult(std::string description) {
  mojom::ActionResultPtr result = MakeScriptFailedResult(description);
  result->execute_js_response = mojom::ExecuteJsResponse::New(
      /*result=*/std::string(), /*truncated=*/false, std::move(description));
  return result;
}

// Returns the result for a script whose promise was rejected with `error`. The
// description of `error` is both the failure message and `js_error_message`.
mojom::ActionResultPtr MakeJsErrorResult(v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::Local<v8::Value> error,
                                         base::TimeTicks deadline) {
  return MakeJsErrorResult(DescribeError(isolate, context, error, deadline));
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
  {
    // Also bounds the microtasks the script queues, since they run before
    // script execution returns. Later continuations, e.g. after a timer or a
    // fetch, run as regular page tasks.
    ScopedTimeoutWatchdog watchdog(isolate, timeout);
    CompiledScript compiled =
        CompileScript(isolate, context, script_code, &timed_out);
    line_offset = compiled.line_offset;
    if (!timed_out && !compiled.script.IsEmpty()) {
      v8::MicrotasksScope microtasks_scope(context,
                                           v8::MicrotasksScope::kRunMicrotasks);
      std::ignore = compiled.script->Run(context).ToLocal(&value);
    } else if (!timed_out) {
      compile_error = std::move(compiled.error_message);
    }
    timed_out = watchdog.Stop() || timed_out || try_catch.HasTerminated();
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

  if (!compile_error.empty()) {
    std::move(callback).Run(MakeJsErrorResult(std::move(compile_error)));
    return;
  }

  if (try_catch.HasCaught()) {
    std::optional<std::string> description =
        FormatTryCatchMessage(isolate, context, try_catch, line_offset);
    if (description) {
      std::move(callback).Run(MakeJsErrorResult(std::move(*description)));
    } else {
      std::move(callback).Run(
          MakeJsErrorResult(isolate, context, try_catch.Exception(), deadline));
    }
    return;
  }

  if (value.IsEmpty()) {
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
        promise->MarkAsHandled();
        std::move(callback).Run(
            MakeJsErrorResult(isolate, context, promise->Result(), deadline));
        return;
      case v8::Promise::kPending: {
        const uint32_t id = AddPendingPromise({std::move(callback), deadline});
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
