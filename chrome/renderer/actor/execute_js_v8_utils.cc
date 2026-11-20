// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/execute_js_v8_utils.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "components/actor/core/actor_util.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-value.h"

namespace actor {
namespace {

// Normalizes values that JSON.stringify() would otherwise serialize to "{}"
// (DOM Elements, NodeLists, HTMLCollections, Errors, Maps, Sets, RegExps)
// before stringifying. Runs inside the actor's isolated world without touching
// globals.
constexpr char kResultNormalizerScript[] = R"js(
(function(root, forConsoleArg) {
  const kMaxCollectionItems = 100;
  const kMaxTextLength = 200;
  const kMaxDepth = 32;
  const kMaxNodes = forConsoleArg ? 2048 : 20000;
  const seen = new WeakSet();
  let nodeCount = 0;

  // Null-prototype descriptor that shadows inherited Object.prototype.toJSON
  // and Array.prototype.toJSON on normalized arrays.
  const kSafeArrayToJsonDesc = Object.create(null);
  kSafeArrayToJsonDesc.value = undefined;

  function makeSafeArray(arr = []) {
    Object.defineProperty(arr, 'toJSON', kSafeArrayToJsonDesc);
    return arr;
  }

  function unserializable(depth, err) {
    if (forConsoleArg && depth === 0) {
      throw err;
    }
    return '[Unserializable]';
  }

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
    const out = Object.create(null);
    out.tag = elTagName.call(el).toLowerCase();
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
        const center = Object.create(null);
        center.x = Math.round(((left + rect.width / 2) / vw) * 999);
        center.y = Math.round(((top + rect.height / 2) / vh) * 999);
        out.center_0_999 = center;
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
        return unserializable(depth, e);
      }
    }
    let tag = '';
    try {
      tag = Object.prototype.toString.call(v);
    } catch (e) {
      return unserializable(depth, e);
    }
    if (v instanceof Error || tag === '[object Error]' ||
        tag === '[object DOMException]') {
      if (forConsoleArg && depth === 0) {
        throw v;
      }
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
      const errOut = Object.create(null);
      errOut.name = name;
      errOut.message = message;
      return errOut;
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
        const out = makeSafeArray();
        for (let i = 0; i < len; ++i) {
          if (nodeCount >= kMaxNodes) {
            out.push('[Truncated]');
            break;
          }
          try {
            out.push(normalize(collectionItem.call(v, i), depth + 1));
          } catch (e) {
            ++nodeCount;
            out.push('[Unserializable]');
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
          const out = makeSafeArray();
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
          const out = makeSafeArray();
          for (const [k, val] of iter) {
            if (nodeCount >= kMaxNodes) {
              out.push('[Truncated]');
              break;
            }
            out.push(makeSafeArray(
                [normalize(k, depth + 1), normalize(val, depth + 1)]));
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
        return unserializable(depth, e);
      }
      if (toJsonFn) {
        try {
          const json = toJsonFn.call(v);
          if (json !== v) {
            return normalize(json, depth + 1);
          }
        } catch (e) {
          return unserializable(depth, e);
        }
      }
      if (Array.isArray(v)) {
        let len = 0;
        try {
          len = v.length;
        } catch (e) {
          return unserializable(depth, e);
        }
        const out = makeSafeArray();
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
        return unserializable(depth, e);
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

}  // namespace

// State shared by a ScopedTimeoutWatchdog and its timeout task, which runs on
// the thread pool. A shared lock serializes TerminateExecution() and
// CancelTerminateExecution() across overlapping watchdogs on the same isolate
// so an inner watchdog cannot cancel another still-active watchdog's timeout.
class ScopedTimeoutWatchdog::WatchdogState
    : public base::RefCountedThreadSafe<WatchdogState> {
 public:
  explicit WatchdogState(v8::Isolate* isolate) : isolate_(isolate) {}

  WatchdogState(const WatchdogState&) = delete;
  WatchdogState& operator=(const WatchdogState&) = delete;

  static void Register(v8::Isolate* isolate,
                       scoped_refptr<WatchdogState> state) {
    base::AutoLock lock(GetLock());
    GetActiveWatchdogs()[isolate].push_back(std::move(state));
  }

  static bool HasActive(v8::Isolate* isolate) {
    base::AutoLock lock(GetLock());
    const auto& active = GetActiveWatchdogs();
    auto it = active.find(isolate);
    return it != active.end() && !it->second.empty();
  }

  void OnTimeout() {
    base::AutoLock lock(GetLock());
    if (cancelled_ || !isolate_) {
      return;
    }
    terminated_ = true;
    isolate_->TerminateExecution();
  }

  // Unregisters this watchdog and prevents any future OnTimeout() from
  // terminating execution. If this watchdog terminated execution and no other
  // still-active watchdog on `isolate` has also terminated, cancels V8's
  // pending termination under the same lock so it cannot race with another
  // watchdog's OnTimeout(). Returns whether this watchdog terminated script.
  bool CancelAndUnregister(v8::Isolate* isolate) {
    base::AutoLock lock(GetLock());
    cancelled_ = true;
    isolate_ = nullptr;
    bool other_terminated = false;
    auto& active = GetActiveWatchdogs();
    if (auto it = active.find(isolate); it != active.end()) {
      auto& states = it->second;
      std::erase(states, this);
      for (const auto& other : states) {
        if (other->terminated_) {
          other_terminated = true;
          break;
        }
      }
      if (states.empty()) {
        active.erase(it);
      }
    }
    if (terminated_ && !other_terminated) {
      isolate->CancelTerminateExecution();
    }
    return terminated_;
  }

 private:
  friend class base::RefCountedThreadSafe<WatchdogState>;
  ~WatchdogState() = default;

  static base::Lock& GetLock() {
    static base::NoDestructor<base::Lock> lock;
    return *lock;
  }

  static base::flat_map<v8::Isolate*,
                        std::vector<scoped_refptr<WatchdogState>>>&
  GetActiveWatchdogs() EXCLUSIVE_LOCKS_REQUIRED(GetLock()) {
    static base::NoDestructor<
        base::flat_map<v8::Isolate*, std::vector<scoped_refptr<WatchdogState>>>>
        active;
    return *active;
  }

  // Cleared in CancelAndUnregister() while the isolate is alive so delayed
  // timeout tasks never retain a dangling raw_ptr after the watchdog finishes.
  raw_ptr<v8::Isolate> isolate_ GUARDED_BY(GetLock());
  bool cancelled_ GUARDED_BY(GetLock()) = false;
  bool terminated_ GUARDED_BY(GetLock()) = false;
};

ScopedTimeoutWatchdog::ScopedTimeoutWatchdog(v8::Isolate* isolate,
                                             base::TimeDelta timeout)
    : isolate_(isolate), state_(base::MakeRefCounted<WatchdogState>(isolate)) {
  WatchdogState::Register(isolate, state_);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WatchdogState::OnTimeout, state_), timeout);
}

ScopedTimeoutWatchdog::~ScopedTimeoutWatchdog() {
  Stop();
}

// static
bool ScopedTimeoutWatchdog::HasActiveWatchdog(v8::Isolate* isolate) {
  return WatchdogState::HasActive(isolate);
}

bool ScopedTimeoutWatchdog::Stop() {
  if (!stopped_) {
    stopped_ = true;
    terminated_ = state_->CancelAndUnregister(isolate_);
  }
  return terminated_;
}

ScriptValueText ToUtf8(v8::Isolate* isolate,
                       v8::Local<v8::String> str,
                       size_t max_bytes) {
  const size_t utf16_length = static_cast<size_t>(str->Length());
  if (max_bytes == 0) {
    return {.truncated = utf16_length > 0};
  }
  // Every UTF-16 code unit encodes to at least 1 UTF-8 byte, so any string
  // longer than `max_bytes` code units is guaranteed to truncate. Read only a
  // bounded UTF-16 prefix via `Write()` so huge V8 ConsString ropes are not
  // flattened just to discard the tail.
  if (utf16_length > max_bytes) {
    std::u16string prefix(max_bytes, u'\0');
    str->Write(isolate, /*offset=*/0, base::checked_cast<uint32_t>(max_bytes),
               reinterpret_cast<uint16_t*>(prefix.data()));
    std::string encoded = base::UTF16ToUTF8(prefix);
    TruncateValidUtf8ToByteSize(encoded, max_bytes);
    return {.text = std::move(encoded), .truncated = true};
  }
  const size_t length = str->Utf8Length(isolate);
  ScriptValueText utf8;
  utf8.text.resize(std::min(length, max_bytes));
  utf8.text.resize(str->WriteUtf8(isolate, utf8.text.data(), utf8.text.size(),
                                  v8::String::WriteFlags::kReplaceInvalidUtf8));
  utf8.truncated = utf8.text.size() < length;
  return utf8;
}

v8::MaybeLocal<v8::Value> NormalizeResultValue(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    v8::Local<v8::Function>* cached_fn,
    bool for_console_arg) {
  v8::Local<v8::Function> fn;
  if (cached_fn && !cached_fn->IsEmpty()) {
    fn = *cached_fn;
  } else {
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8Literal(isolate, kResultNormalizerScript);
    v8::Local<v8::Script> script;
    v8::Local<v8::Value> fn_value;
    if (!v8::Script::Compile(context, source).ToLocal(&script) ||
        !script->Run(context).ToLocal(&fn_value) || !fn_value->IsFunction()) {
      return {};
    }
    fn = fn_value.As<v8::Function>();
    if (cached_fn) {
      *cached_fn = fn;
    }
  }
  v8::Local<v8::Value> argv[] = {value,
                                 v8::Boolean::New(isolate, for_console_arg)};
  return fn->Call(context, v8::Undefined(isolate), std::size(argv), argv);
}

}  // namespace actor
