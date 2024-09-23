// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/win/delay_load_notify_hook.h"

#include <atomic>

namespace chrome {

namespace {

std::atomic<DelayLoadCallbackFunction> g_delay_load_callback_function(nullptr);

// This function is a delay load notification hook. It is invoked by the
// delay load support in the visual studio runtime. Override the behavior
// using |SetDelayLoadHookCallback|. More details on usage in
// delay_load_notify_hook.h
FARPROC WINAPI DelayLoadNotifyHook(unsigned dliNotify, PDelayLoadInfo pdli) {
  DelayLoadCallbackFunction callback =
      g_delay_load_callback_function.load(std::memory_order_acquire);
  if (!callback) {
    return 0;
  }
  return callback(dliNotify, pdli);
}

}  // namespace

void SetDelayLoadHookCallback(DelayLoadCallbackFunction callback_function) {
  g_delay_load_callback_function = callback_function;
}

}  // namespace chrome

// Set the delay load hook to the function above.
//
// |__pfnDliNotifyHook2| gets called automatically by delay load runtime
// at several points throughout delay loading, providing application the
// ability to modify delayload behavior. See
// https://learn.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function
extern "C" const PfnDliHook __pfnDliNotifyHook2 = chrome::DelayLoadNotifyHook;
