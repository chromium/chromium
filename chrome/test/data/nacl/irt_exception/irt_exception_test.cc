// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <setjmp.h>
#include <stdio.h>

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/include/nacl/nacl_exception.h"

namespace {

jmp_buf g_jmp_buf;

void MyNaClExceptionHandler(struct NaClExceptionContext* context) {
  printf("--- MyNaClExceptionHandler\n");
  longjmp(g_jmp_buf, 1);
}

void CrashViaSignalHandler() {
  printf("--- CrashViaSignalHandler\n");

  int retval = nacl_exception_set_handler(MyNaClExceptionHandler);
  if (retval != 0) {
    printf("Unexpected return value from nacl_exception_set_handler: %d\n",
           retval);
    TEST_FAILED;
    return;
  }

  if (setjmp(g_jmp_buf)) {
    printf("Returned via longjmp\n");
    TEST_PASSED;
    return;
  }
  printf("Going to crash\n");
  __builtin_trap();
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashViaSignalHandler", CrashViaSignalHandler);
}

void SetupPluginInterfaces() {}
