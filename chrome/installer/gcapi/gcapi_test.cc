// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "testing/gtest/include/gtest/gtest.h"

void call_statically() {
  DWORD reason = 0;
  BOOL result_flag_on = FALSE;
  BOOL result_flag_off = FALSE;

  // running this twice verifies that the first call does not set
  // a flag that would make the second fail.  Thus, the results
  // of the two calls should be the same (no state should have changed)
  result_flag_off = GoogleChromeCompatibilityCheck(
      FALSE, GCAPI_INVOKED_STANDARD_SHELL, &reason);
  result_flag_on = GoogleChromeCompatibilityCheck(
      TRUE, GCAPI_INVOKED_STANDARD_SHELL, &reason);

  if (result_flag_off != result_flag_on)
    printf("Registry key flag is not being set properly.");

  printf("Static call returned result as %d and reason as %ld.\n",
         result_flag_on, reason);
}

void call_dynamically() {
  HMODULE module = LoadLibrary(L"gcapi_dll.dll");
  if (module == nullptr) {
    printf("Couldn't load gcapi_dll.dll.\n");
    return;
  }

  GCCC_CompatibilityCheck gccfn = (GCCC_CompatibilityCheck)GetProcAddress(
      module, "GoogleChromeCompatibilityCheck");
  if (gccfn != nullptr) {
    DWORD reason = 0;

    // running this twice verifies that the first call does not set
    // a flag that would make the second fail.  Thus, the results
    // of the two calls should be the same (no state should have changed)
    BOOL result_flag_off = gccfn(FALSE, GCAPI_INVOKED_STANDARD_SHELL, &reason);
    BOOL result_flag_on = gccfn(TRUE, GCAPI_INVOKED_STANDARD_SHELL, &reason);

    if (result_flag_off != result_flag_on)
      printf("Registry key flag is not being set properly.");

    printf("Dynamic call returned result as %d and reason as %ld.\n",
           result_flag_on, reason);
  } else {
    printf("Couldn't find GoogleChromeCompatibilityCheck() in gcapi_dll.\n");
  }
  FreeLibrary(module);
}

const char kManualLaunchTests[] = "launch-chrome";

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  install_static::ScopedInstallDetails install_details;

  testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kManualLaunchTests)) {
    call_dynamically();
    call_statically();
    printf("LaunchChrome returned %d.\n", LaunchGoogleChrome());
  }

  return ret;
}
