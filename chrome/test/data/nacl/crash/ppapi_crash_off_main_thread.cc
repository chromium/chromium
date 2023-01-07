// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test that we handle crashes on threads other than main.

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"

namespace {

void* CrashOffMainThreadFunction(void* thread_arg) {
  printf("--- CrashOffMainThreadFunction\n");
  usleep(1000);  // Try to wait until PPP_Messaging::HandleMessage returns.
  CRASH;
  return NULL;
}

// Depending on how the detached thread is scheduled, this will either crash
// during PPP_Messaging::HandleMessage or after it and before the next PPP call.
// If a crash occurs within a PPP call, it returns an RPC error.
// If a crash occurs between PPP calls, the next call will return an RPC error.
void CrashOffMainThread() {
  printf("--- CrashOffMainThread\n");
  pthread_t tid;
  pthread_create(&tid, NULL /*attr*/, CrashOffMainThreadFunction, NULL);
  pthread_detach(tid);
}

// This will allow us to ping the nexe to detect a crash that occured
// while the main thread was waiting for the next PPP call.
void Ping() {
  LOG_TO_BROWSER("ping received");
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashOffMainThread", CrashOffMainThread);
  RegisterTest("Ping", Ping);
}

void SetupPluginInterfaces() {
  // none
}
