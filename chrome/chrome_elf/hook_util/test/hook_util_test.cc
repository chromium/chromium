// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/chrome_elf/hook_util/hook_util.h"
// Compile in this test DLL, so that it's in the IAT.
#include "chrome/chrome_elf/hook_util/test/hook_util_test_dll.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// IATHook test constants.
const char kIATTestDllName[] = "hook_util_test_dll.dll";
const char kIATExportedApiFunction[] = "ExportedApi";

// IATHook function, which does nothing.
void IATHookedExportedApi() {
  return;
}

// Shady third-party IATHook function, which also does nothing, but different
// chunk of code/address.
void IATHookedExportedApiTwo() {
  printf("Something to make this function different!\n");
  return;
}

class HookTest : public testing::Test {
 protected:
  HookTest() {}
};

//------------------------------------------------------------------------------
// IATHook tests
//------------------------------------------------------------------------------

TEST_F(HookTest, IATHook) {
  // Sanity test with no hook.
  ASSERT_EQ(0, ExportedApiCallCount());
  ExportedApi();
  ExportedApi();
  ASSERT_EQ(2, ExportedApiCallCount());

  // Apply IAT hook.
  elf_hook::IATHook iat_hook;
  if (iat_hook.Hook(
          ::GetModuleHandle(nullptr), kIATTestDllName, kIATExportedApiFunction,
          reinterpret_cast<void*>(IATHookedExportedApi)) != NO_ERROR) {
    ADD_FAILURE();
    return;
  }

  // Make sure hooking twice with the same object fails.
  if (iat_hook.Hook(::GetModuleHandle(nullptr), kIATTestDllName,
                    kIATExportedApiFunction,
                    reinterpret_cast<void*>(IATHookedExportedApi)) !=
      ERROR_SHARING_VIOLATION)
    ADD_FAILURE();

  // Call count should not change with hook.
  ExportedApi();
  ExportedApi();
  ExportedApi();
  EXPECT_EQ(2, ExportedApiCallCount());

  // Remove hook.
  if (iat_hook.Unhook() != NO_ERROR)
    ADD_FAILURE();

  // Sanity test things are back to normal.
  ExportedApi();
  EXPECT_EQ(3, ExportedApiCallCount());

  // Double unhook should fail.
  if (iat_hook.Unhook() != ERROR_INVALID_PARAMETER)
    ADD_FAILURE();

  // Try hooking a non-existent function.
  if (iat_hook.Hook(::GetModuleHandle(nullptr), kIATTestDllName, "FooBarred",
                    reinterpret_cast<void*>(IATHookedExportedApi)) !=
      ERROR_PROC_NOT_FOUND)
    ADD_FAILURE();

  // Test the case where someone else hooks our hook!  Unhook() should leave it.
  if (iat_hook.Hook(
          ::GetModuleHandle(nullptr), kIATTestDllName, kIATExportedApiFunction,
          reinterpret_cast<void*>(IATHookedExportedApi)) != NO_ERROR) {
    ADD_FAILURE();
    return;
  }
  elf_hook::IATHook shady_third_party_iat_hook;
  if (shady_third_party_iat_hook.Hook(
          ::GetModuleHandle(nullptr), kIATTestDllName, kIATExportedApiFunction,
          reinterpret_cast<void*>(IATHookedExportedApiTwo)) != NO_ERROR)
    ADD_FAILURE();
  if (iat_hook.Unhook() != ERROR_INVALID_FUNCTION)
    ADD_FAILURE();
  if (shady_third_party_iat_hook.Unhook() != NO_ERROR)
    ADD_FAILURE();
  // NOTE: the first hook was left in and couldn't be cleaned up.
}

}  // namespace
