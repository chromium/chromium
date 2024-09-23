// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/win/delay_load_notify_hook.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

constexpr char kTestDll[] = "test.dll";
constexpr char kDummyFunction[] = "test.dll";

void WINAPI DummyFunction() {}

FARPROC TestDelayLoadCallbackFunction(unsigned delay_load_event,
                                      DelayLoadInfo* delay_load_info) {
  if (delay_load_event == dliNotePreGetProcAddress &&
      strcmp(delay_load_info->szDll, kTestDll) == 0 &&
      strcmp(delay_load_info->dlp.szProcName, kDummyFunction) == 0) {
    return reinterpret_cast<FARPROC>(DummyFunction);
  }
  return nullptr;
}

}  // namespace

// This test verifies that delay load hooks are correctly in place for the
// current module, which is unit_tests.exe in the case of this test.
TEST(ChromeDelayLoadNotifyHookTest, HooksAreSetAtLinkTime) {
  ASSERT_NE(__pfnDliNotifyHook2, nullptr);
}

// This test verifies a typical usage of SetDelayLoadHookCallback
TEST(ChromeDelayLoadNotifyHookTest, OverrideDliNotifyHook) {
  DelayLoadInfo dli = {.szDll = kTestDll};
  dli.dlp.szProcName = kDummyFunction;
  absl::Cleanup reset_callback = [] {
    chrome::SetDelayLoadHookCallback(nullptr);
  };
  chrome::SetDelayLoadHookCallback(&TestDelayLoadCallbackFunction);
  EXPECT_EQ(__pfnDliNotifyHook2(dliNotePreGetProcAddress, &dli),
            reinterpret_cast<FARPROC>(DummyFunction));
}
