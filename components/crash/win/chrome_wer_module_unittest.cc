// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <werapi.h>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash {
namespace test {
namespace {
base::FilePath ModulePath() {
  return base::FilePath(FILE_PATH_LITERAL("chrome_wer.dll"));
}

// Check that chrome_wer.dll has expected exports and loads.
// The module is usually loaded by WerFault.exe after a crash.
TEST(ChromeWerModule, ValidModule) {
  // Module loads.
  HMODULE hMod = LoadLibraryW(ModulePath().value().c_str());
  ASSERT_TRUE(hMod);

  // Required functions exist.
  auto wref = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_EVENT>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_EVENT_FUNCTION));
  ASSERT_TRUE(wref);
  auto wrees = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_EVENT_SIGNATURE>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_EVENT_SIGNATURE_FUNCTION));
  ASSERT_TRUE(wrees);
  auto wredl = reinterpret_cast<PFN_WER_RUNTIME_EXCEPTION_DEBUGGER_LAUNCH>(
      GetProcAddress(hMod, WER_RUNTIME_EXCEPTION_DEBUGGER_LAUNCH));
  ASSERT_TRUE(wredl);

  // Call each function to verify the calling convention.
  // Not-implemented functions return E_FAIL as expected.
  HRESULT res = wrees(nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, E_FAIL);
  res = wredl(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, E_FAIL);

  WER_RUNTIME_EXCEPTION_INFORMATION wer_ex;
  BOOL claimed = TRUE;

  // No registration context => skip.
  res = wref(nullptr, &wer_ex, &claimed, nullptr, nullptr, nullptr);
  ASSERT_EQ(res, S_OK);
  ASSERT_EQ(claimed, FALSE);

  FreeLibrary(hMod);
}

}  // namespace
}  // namespace test
}  // namespace crash
