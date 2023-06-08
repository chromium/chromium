// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"

#include <vector>

namespace {

// This will crash a PPP_Messaging function.
void CrashViaLibcxxAssertFailure() {
  printf("--- CrashViaLibcxxAssertFailure\n");
  std::vector<int> v = {0};
  v[0] = v[1];
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashViaLibcxxAssertFailure", CrashViaLibcxxAssertFailure);
}

void SetupPluginInterfaces() {
  // none
}
