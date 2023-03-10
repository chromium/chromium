// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <delayimp.h>

#include "base/process/memory.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Subtle: In tests, unit_tests.exe is linked with
// chrome/common/win/delay_load_failure_hook.cc not
// chrome/app/delay_load_failure_hook_win.cc. So, __pfnDliFailureHook2 will
// always call DelayLoadFailureHook and not DelayLoadFailureHookEXE despite
// existing in an exe file named unit_tests.exe.
//
// In production chrome.exe, __pfnDliFailureHook2 is instead backed by
// DelayLoadFailureHookEXE in chrome/app/delay_load_failure_hook_win.cc, while
// in chrome.dll, __pfnDliFailureHook2 is backed by DelayLoadFailureHook from
// chrome/common/win/delay_load_failure_hook.cc.

// This test verifies that delay load hooks are correctly in place for the
// current module, which is unit_tests.exe in the case of this test.
TEST(ChromeDelayLoadHookTest, HooksAreSetAtLinkTime) {
  ASSERT_NE(__pfnDliFailureHook2, nullptr);
}

// This test verifies DelayLoadFailureHook crashes for a failure.
TEST(ChromeDelayLoadHookTest, DllLoadFailureCrashes) {
  DelayLoadInfo dli = {.szDll = "test.dll"};
  EXPECT_CHECK_DEATH({ __pfnDliFailureHook2(dliFailLoadLib, &dli); });
}

// This test verifies that if a DLL is failing to load because of lack of
// memory, an OOM exception is generated rather than just a CHECK so as to
// distinguish them in crash reports.
TEST(ChromeDelayLoadHookDeathTest, OomIsHandled) {
  DelayLoadInfo dli = {.szDll = "test.dll",
                       .dwLastError = ERROR_COMMITMENT_LIMIT};
  const auto expected = base::StringPrintf("Received fatal exception 0x%08lx",
                                           base::win::kOomExceptionCode);
  EXPECT_DEATH_IF_SUPPORTED({ __pfnDliFailureHook2(dliFailLoadLib, &dli); },
                            expected);
}
