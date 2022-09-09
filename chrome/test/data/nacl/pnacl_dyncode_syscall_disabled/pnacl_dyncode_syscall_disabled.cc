// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <nacl/nacl_dyncode.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

namespace {

void TestIrtInterfaceHidden(void) {
  struct nacl_irt_dyncode interface;
  size_t result = __nacl_irt_query(NACL_IRT_DYNCODE_v0_1,
                                   &interface, sizeof(interface));
  EXPECT(result == 0);

  TEST_PASSED;
}

void TestDyncodeCreate(void) {
  EXPECT(nacl_dyncode_create(NULL, NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

void TestDyncodeModify(void) {
  EXPECT(nacl_dyncode_modify(NULL, NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

void TestDyncodeDelete(void) {
  EXPECT(nacl_dyncode_delete(NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestIrtInterfaceHidden", TestIrtInterfaceHidden);
  RegisterTest("TestDyncodeCreate", TestDyncodeCreate);
  RegisterTest("TestDyncodeModify", TestDyncodeModify);
  RegisterTest("TestDyncodeDelete", TestDyncodeDelete);
}

void SetupPluginInterfaces() {
  // none
}
