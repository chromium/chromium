// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"

namespace {

// This will crash a PPP_Messaging function.
void CrashViaExitCall() {
  printf("--- CrashViaExitCall\n");
  _exit(0);
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashViaExitCall", CrashViaExitCall);
}

void SetupPluginInterfaces() {
  // none
}
