// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_EXECUTE_JS_CONSOLE_CAPTURE_H_
#define CHROME_RENDERER_ACTOR_EXECUTE_JS_CONSOLE_CAPTURE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace actor {

struct ConsoleCaptureState;

// Temporarily wraps `console.{log,info,warn,error}` inside the actor's
// isolated world to capture script output (capped at
// `kExecuteJsMaxConsoleLines` lines and `kExecuteJsMaxConsoleBytes` bytes)
// without affecting the page's main world. Restores the original console
// methods on TakeLogsAndRestore() or destruction (or on ScopedActiveWatchdog
// destruction if termination raced with destruction during a nested run loop).
class ConsoleCapture {
 public:
  // Marks `capture` as running under an outer ScopedTimeoutWatchdog while in
  // scope, keeping the ref-counted state alive on the stack even if a nested
  // run loop cancels or destroys the owning ExecuteJsTool. Must outlive the
  // outer ScopedTimeoutWatchdog so that its destructor runs after termination
  // is cancelled and completes any deferred `console.*` restoration. Safe when
  // `capture` is null.
  class ScopedActiveWatchdog {
   public:
    explicit ScopedActiveWatchdog(ConsoleCapture* capture);
    ScopedActiveWatchdog(const ScopedActiveWatchdog&) = delete;
    ScopedActiveWatchdog& operator=(const ScopedActiveWatchdog&) = delete;
    ~ScopedActiveWatchdog();

   private:
    raw_ptr<v8::Isolate> isolate_ = nullptr;
    scoped_refptr<ConsoleCaptureState> state_;
    bool previous_value_ = false;
  };

  // Installs wrappers on `console.{log,info,warn,error}` in `context`. Caller
  // must hold a v8::HandleScope, v8::Context::Scope for `context`, and
  // v8::TryCatch (V8 termination is rethrown to it). The returned capture must
  // be destroyed before `isolate` is torn down.
  static std::unique_ptr<ConsoleCapture> Install(v8::Isolate* isolate,
                                                 v8::Local<v8::Context> context,
                                                 base::TimeTicks deadline);

  ConsoleCapture(const ConsoleCapture&) = delete;
  ConsoleCapture& operator=(const ConsoleCapture&) = delete;

  ~ConsoleCapture();

  std::string TakeLogsAndRestore();

 private:
  ConsoleCapture(v8::Isolate* isolate,
                 uint32_t capture_id,
                 scoped_refptr<ConsoleCaptureState> state);

  void Restore();

  const raw_ptr<v8::Isolate> isolate_;
  const uint32_t capture_id_;
  scoped_refptr<ConsoleCaptureState> state_;
  bool restored_ = false;
};

// Restores `capture` (if non-null), destroys it, and returns any captured
// console logs (empty if none were captured).
std::string TakeConsoleLogs(std::unique_ptr<ConsoleCapture> capture);

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_EXECUTE_JS_CONSOLE_CAPTURE_H_
