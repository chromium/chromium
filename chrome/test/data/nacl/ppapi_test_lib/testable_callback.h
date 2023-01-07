// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TESTABLE_CALLBACK_H_
#define CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TESTABLE_CALLBACK_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"

// NOTE: if you use you TestableCallback you will need to enable
// testing interfaces in PPAPIBrowserTester(), e.g.
// "--enable-pepper-testing", c.f.
// tests/ppapi_browser/ppb_graphics2d/nacl.scons
//
// Example Usage:
//
// void TestProgressSimple() {
//   TestableCallback callback(pp_instance(), true);
//   ...
//   rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
//   EXPECT(rv == PP_OK_COMPLETIONPENDING);

//   rv = callback.WaitForResult();
//   EXPECT(rv == PP_OK);
//   ...
// }

class TestableCallback {
 public:
  TestableCallback(PP_Instance instance, bool force_async);

  // Get the callback to be passed to an asynchronous PPAPI call
  PP_CompletionCallback GetCallback();

  // Waits for the callback to be called and returns the
  // result. Returns immediately if the callback was previously called
  // and the result wasn't returned (i.e. each result value received
  // by the callback is returned by WaitForResult() once and only
  // once).
  int32_t WaitForResult();

  bool HasRun() const { return run_count_ != 0; }

  // Make instance runnable again.
  void Reset() {
    run_count_ = 0;
    have_result_ = false;
  }

  int32_t Result() const { return result_; }

 private:
  static void Handler(void* user_data, int32_t result);

  bool have_result_;       // is a result available?
  int32_t result_;         // value of the result
  bool force_async_;       // force callback to be always called
  bool post_quit_task_;    // has cleanup beem performed
  unsigned run_count_;     // number of times the callback has been called
  PP_Instance instance_;
};

#endif  // CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TESTABLE_CALLBACK_H_
