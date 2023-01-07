// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/testable_callback.h"

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_testing_private.h"

TestableCallback::TestableCallback(PP_Instance instance, bool force_async)
    : have_result_(false),
      result_(PP_OK_COMPLETIONPENDING),
      force_async_(force_async),
      post_quit_task_(false),
      run_count_(0),
      instance_(instance) {
}

int32_t TestableCallback::WaitForResult() {
  if (!have_result_) {
    result_ = PP_OK_COMPLETIONPENDING;  // Reset
    post_quit_task_ = true;

    // This waits until PPBTestingDev()->QuitMessageLoop() is called
    // by the "Handler" which represents the actual callback code.
    PPBTestingPrivate()->RunMessageLoop(instance_);
  }
  have_result_ = false;
  return result_;
}

PP_CompletionCallback TestableCallback::GetCallback() {
  int32_t flags = force_async_ ? 0 : PP_COMPLETIONCALLBACK_FLAG_OPTIONAL;
  PP_CompletionCallback cc =
    PP_MakeCompletionCallback(&TestableCallback::Handler, this);
  cc.flags = flags;
  return cc;
}

// static, so we can take it's address
// This is the actual callback, all it does is record
// the result and wake up whoever is block on
// "WaitForResult"
void TestableCallback::Handler(void* user_data, int32_t result) {
  TestableCallback* callback = static_cast<TestableCallback*>(user_data);
  callback->result_ = result;
  callback->have_result_ = true;
  ++callback->run_count_;
  if (callback->post_quit_task_) {
    callback->post_quit_task_ = false;
    PPBTestingPrivate()->QuitMessageLoop(callback->instance_);
  }
}
