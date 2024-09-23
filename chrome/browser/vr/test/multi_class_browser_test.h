// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MULTI_CLASS_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_MULTI_CLASS_BROWSER_TEST_H_

#include "content/public/test/browser_test.h"
#include "device/vr/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

// A collection of macros to automatically run a browser test multiple times
// with different test classes/test fixtures. These are used the same way as
// the standard IN_PROC_BROWSER_TEST_F and IN_PROC_BROWSER_TEST_P macros, except
// that multiple test classes/fixtures are specified instead of one and a common
// base class must be provided. The number suffix indicates how many classes the
// macro expects, e.g. IN_PROC_MULTI_CLASS_BROWSER_TEST_F2 takes two classes.

// One important note is that while IN_PROC_BROWSER_TEST_F would normally
// provide the constructed test class as "this", the reference to the
// constructed class when using the multi class macros is instead provided as
// "t". t's type will also be the provided base class, so if you need behavior
// specific to one of the subclasses, you must first cast it. Although in such
// cases, you should probably reconsider whether use of these macros is really
// appropriate instead of having separate implementations.

// In essence:
//
// IN_PROC_MULTI_CLASS_BROWSER_TEST_F2(sub1, sub2, base, name) { impl }
//
// Is shorthand for:
//
// void TestImpl(base* t) { impl }
// IN_PROC_BROWSER_TEST_F(sub1, name) { TestImpl(this); }
// IN_PROC_BROWSER_TEST_F(sub2, name) { TestImpl(this); }

// We essentially create a dummy class for the test which contains a declaration
// for a static function that will have the actual test implementation. We then
// define as many IN_PROC_BROWSER_TEST_Fs as necesasry and have each call the
// static function with its "this" reference. Finally, we start the definition
// for the static function but leave it unfinished so that the definition
// provided by the caller is used (which is also what IN_PROC_BROWSER_TEST_F
// does under the hood).

#define MULTI_CLASS_RUNNER_NAME_(test_name) RunMultiClassBrowserTest_##test_name

#define DEFINE_RUN_TEST_IMPL_(test_name, base_class)        \
  class MULTI_CLASS_RUNNER_NAME_(test_name) {               \
   public:                                                  \
    MULTI_CLASS_RUNNER_NAME_(test_name)() {}                \
    static void ActuallyRunTestOnMainThread(base_class* t); \
  };

#define DEFINE_BROWSER_TEST_(test_class, test_name)                         \
  IN_PROC_BROWSER_TEST_F(test_class, test_name) {                           \
    MULTI_CLASS_RUNNER_NAME_(test_name)::ActuallyRunTestOnMainThread(this); \
  }

#define DEFINE_INCOGNITO_BROWSER_TEST_(test_class, test_name)               \
  IN_PROC_BROWSER_TEST_F(test_class, test_name##Incognito) {                \
    SetIncognito();                                                         \
    MULTI_CLASS_RUNNER_NAME_(test_name)::ActuallyRunTestOnMainThread(this); \
  }

#define IN_PROC_NORMAL_PLUS_INCOGNITO_BROWSER_TEST_F(test_class1, base_class, \
                                                     test_name)               \
  DEFINE_RUN_TEST_IMPL_(test_name, base_class)                                \
  DEFINE_BROWSER_TEST_(test_class1, test_name)                                \
  DEFINE_INCOGNITO_BROWSER_TEST_(test_class1, test_name)                      \
  void MULTI_CLASS_RUNNER_NAME_(test_name)::ActuallyRunTestOnMainThread(      \
      base_class* t)

// TODO(crbug.com/40736732): The "MULTI_CLASS" macros are not really
// needed anymore, and the individual tests should be wrapped with a check to
// the openxr buildflag. However, there is a non-trivial amount of churn to move
// the tests off of the "ALL_RUNTIMES" macros. So this lets us stage the work in
// the meantime.
#define MULTI_CLASS_BROWSER_TEST_STUB(base_class, test_name)             \
  DEFINE_RUN_TEST_IMPL_(test_name, base_class)                           \
  void MULTI_CLASS_RUNNER_NAME_(test_name)::ActuallyRunTestOnMainThread( \
      base_class* t)

#define IN_PROC_MULTI_CLASS_BROWSER_TEST_F1(test_class1, base_class,     \
                                            test_name)                   \
  DEFINE_RUN_TEST_IMPL_(test_name, base_class)                           \
  DEFINE_BROWSER_TEST_(test_class1, test_name)                           \
  void MULTI_CLASS_RUNNER_NAME_(test_name)::ActuallyRunTestOnMainThread( \
      base_class* t)

#if BUILDFLAG(ENABLE_OPENXR)
#define WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(test_name)         \
  IN_PROC_MULTI_CLASS_BROWSER_TEST_F1(WebXrVrOpenXrBrowserTest, \
                                      WebXrVrBrowserTestBase, test_name)
#else
#define WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(test_name) \
  MULTI_CLASS_BROWSER_TEST_STUB(WebXrVrBrowserTestBase, test_name)
#endif  // BUILDFLAG(ENABLE_OPENXR)

// The same as WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F, but runs the tests in
// incognito mode as well.
#if BUILDFLAG(ENABLE_OPENXR)
#define WEBXR_VR_ALL_RUNTIMES_PLUS_INCOGNITO_BROWSER_TEST_F(test_name) \
  IN_PROC_NORMAL_PLUS_INCOGNITO_BROWSER_TEST_F(                        \
      WebXrVrOpenXrBrowserTest, WebXrVrBrowserTestBase, test_name)
#else
#define WEBXR_VR_ALL_RUNTIMES_PLUS_INCOGNITO_BROWSER_TEST_F(test_name) \
  MULTI_CLASS_BROWSER_TEST_STUB(WebXrVrBrowserTestBase, test_name)
#endif  // ENABLE_OPENXR

// Helper class to disable a specific runtime of the above
#define WEBXR_VR_DISABLE_TEST_ON(runtime) \
  if (t->GetRuntimeType() == runtime)     \
  return

#endif  // CHROME_BROWSER_VR_TEST_MULTI_CLASS_BROWSER_TEST_H_
