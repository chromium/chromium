// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_EXECUTE_JS_V8_UTILS_H_
#define CHROME_RENDERER_ACTOR_EXECUTE_JS_V8_UTILS_H_

#include <cstddef>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace actor {

class ScriptCanceller;

// Terminates JavaScript running on `isolate` once `timeout` elapses, or when
// `canceller`, if any, cancels it. Scope it to the call that runs page
// JavaScript: stopping or destroying it stops the timeout and the cancellation,
// and clears any termination they requested, so the termination can't hit
// unrelated script that runs later.
class ScopedTimeoutWatchdog {
 public:
  ScopedTimeoutWatchdog(v8::Isolate* isolate,
                        base::TimeDelta timeout,
                        scoped_refptr<ScriptCanceller> canceller = nullptr);

  ScopedTimeoutWatchdog(const ScopedTimeoutWatchdog&) = delete;
  ScopedTimeoutWatchdog& operator=(const ScopedTimeoutWatchdog&) = delete;

  ~ScopedTimeoutWatchdog();

  // Returns whether any ScopedTimeoutWatchdog is currently active on `isolate`.
  static bool HasActiveWatchdog(v8::Isolate* isolate);

  // Returns whether the watchdog terminated script, because the timeout
  // elapsed or the script was cancelled.
  bool Stop();

 private:
  friend class ScriptCanceller;
  class WatchdogState;

  const raw_ptr<v8::Isolate> isolate_;
  const scoped_refptr<WatchdogState> state_;
  const scoped_refptr<ScriptCanceller> canceller_;
  bool stopped_ = false;
  bool terminated_ = false;
};

// Cancels the script that `ScopedTimeoutWatchdog`s watch for it, e.g. the
// script an `ExecuteJsTool` runs. Can be used on any thread, so that the
// browser can cancel script that blocks the main thread. Only terminates script
// while a `ScopedTimeoutWatchdog` watches it, and the lock makes terminating
// and stopping watching mutually exclusive, so cancelling never terminates
// unrelated script.
class ScriptCanceller : public base::RefCountedThreadSafe<ScriptCanceller> {
 public:
  ScriptCanceller();

  ScriptCanceller(const ScriptCanceller&) = delete;
  ScriptCanceller& operator=(const ScriptCanceller&) = delete;

  // Terminates the watched script, if any, and any script watched later.
  void Cancel();

  bool IsCancelled() const;

 private:
  friend class base::RefCountedThreadSafe<ScriptCanceller>;
  friend class ScopedTimeoutWatchdog;

  ~ScriptCanceller();

  // Watches the script that `state`'s watchdog watches until `StopWatching()`.
  // If already cancelled, terminates the script right away. Watchdogs that
  // share a canceller must not overlap: it watches one at a time.
  void StartWatching(scoped_refptr<ScopedTimeoutWatchdog::WatchdogState> state);

  // Once this returns, `Cancel()` no longer terminates script.
  void StopWatching();

  mutable base::Lock lock_;
  bool cancelled_ GUARDED_BY(lock_) = false;
  // Only set while a watchdog watches script, so that cancelling terminates
  // only that script.
  scoped_refptr<ScopedTimeoutWatchdog::WatchdogState> watched_state_
      GUARDED_BY(lock_);
};

struct ScriptValueText {
  std::string text;
  bool truncated = false;
};

// Returns `str` as UTF-8, truncated to at most `max_bytes` bytes.
ScriptValueText ToUtf8(v8::Isolate* isolate,
                       v8::Local<v8::String> str,
                       size_t max_bytes);

// Runs the result normalizer script in `context` on `value` so DOM Elements,
// NodeLists, HTMLCollections, Errors, Maps, Sets, and RegExps serialize
// meaningfully instead of "{}". If `cached_fn` is non-null, reuses/populates
// it across calls. When `for_console_arg` is true, lowers the traversal node
// cap (from 20,000 to 2,048) and throws top-level unserializable errors and
// top-level Error/DOMException values instead of normalizing them so console
// formatting falls back to `ToString()` consistently with native Errors.
// Caller must hold a v8::Context::Scope (or be in `context`),
// v8::MicrotasksScope, v8::TryCatch, and ScopedTimeoutWatchdog.
v8::MaybeLocal<v8::Value> NormalizeResultValue(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    v8::Local<v8::Function>* cached_fn = nullptr,
    bool for_console_arg = false);

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_EXECUTE_JS_V8_UTILS_H_
