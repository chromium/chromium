// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"

int main(int argc, char** argv) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  //
  // We enable secure DLL loading in the test suite to be sure that it doesn't
  // affect the behaviour of functionality that's tested.
  chrome_cleaner::EnableSecureDllLoading();

  // Some cleaner tests need administrator privileges to run.
  if (!chrome_cleaner::CheckTestPrivileges())
    return 1;

  return chrome_cleaner::RunChromeCleanerTestSuite(
      argc, argv, {&chrome_cleaner::TestUwSCatalog::GetInstance()});
}
