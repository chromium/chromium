// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/include/nacl/nacl_exception.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

namespace {

void TestIrtInterfaceHidden(void) {
  struct nacl_irt_exception_handling interface;
  size_t result = __nacl_irt_query(NACL_IRT_EXCEPTION_HANDLING_v0_1,
                                   &interface, sizeof(interface));
  EXPECT(result == 0);

  TEST_PASSED;
}

void TestExceptionSetHandler(void) {
  int retval = nacl_exception_set_handler(NULL);
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

void TestExceptionSetStack(void) {
  int retval = nacl_exception_set_stack(NULL, 0);
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

void TestExceptionClearFlag(void) {
  int retval = nacl_exception_clear_flag();
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestIrtInterfaceHidden", TestIrtInterfaceHidden);
  RegisterTest("TestExceptionSetHandler", TestExceptionSetHandler);
  RegisterTest("TestExceptionSetStack", TestExceptionSetStack);
  RegisterTest("TestExceptionClearFlag", TestExceptionClearFlag);
}

void SetupPluginInterfaces() {
  // none
}
