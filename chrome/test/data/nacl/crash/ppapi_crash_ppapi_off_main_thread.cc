// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test that we kill the nexe on a CHECK and handle it gracefully on the
// trusted side when untrusted code makes unsupported PPAPI calls
// on other than the main thread.

#include <pthread.h>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "ppapi/c/ppb_url_request_info.h"

namespace {

void* CrashOffMainThreadFunction(void* thread_arg) {
  printf("--- CrashPPAPIOffMainThreadFunction\n");
  CRASH;
  return NULL;
}


// This will crash PPP_Messaging::HandleMessage.
void CrashPPAPIOffMainThread() {
  printf("--- CrashPPAPIOffMainThread\n");
  pthread_t tid;
  void* thread_result;
  pthread_create(&tid, NULL /*attr*/, CrashOffMainThreadFunction, NULL);
  pthread_join(tid, &thread_result);  // Wait for the thread to crash.
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashPPAPIOffMainThread", CrashPPAPIOffMainThread);
}

void SetupPluginInterfaces() {
  // none
}
