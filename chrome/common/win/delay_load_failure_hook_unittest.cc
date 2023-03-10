// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <delayimp.h>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeDelayLoadHookTest, HooksAreSetAtLinkTime) {
  // This test verifies that delay load hooks are correctly in place for the
  // current module.
  ASSERT_NE(__pfnDliFailureHook2, nullptr);

  // Subtle: In tests, unit_tests.exe is linked with
  // chrome/common/win/delay_load_failure_hook.cc not
  // chrome/app/delay_load_failure_hook_win.cc. So, __pfnDliFailureHook2 will
  // always call DelayLoadFailureHook and not DelayLoadFailureHookEXE despite
  // existing in unit_tests.exe.
  //
  // In production chrome.exe, __pfnDliFailureHook2 is instead backed by
  // DelayLoadFailureHookEXE in chrome/app/delay_load_failure_hook_win.cc, while
  // in chrome.dll, __pfnDliFailureHook2 is backed by DelayLoadFailureHook from
  // chrome/common/win/delay_load_failure_hook.cc.
  //
  // This test verifies DelayLoadFailureHook crashes.
  DelayLoadInfo dli = {.szDll = "test.dll"};
  EXPECT_CHECK_DEATH({ __pfnDliFailureHook2(dliFailLoadLib, &dli); });
}
