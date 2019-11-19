// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_test.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"


// FullscreenControllerStateInteractiveTest ------------------------------------

// Interactive test fixture testing Fullscreen Controller through its states.
//
// Used to verify that the FullscreenControllerTestWindow models the behavior
// of actual windows accurately. The interactive tests are too flaky to run
// on infrastructure, and so those tests are disabled. Run them with:
//     interactive_ui_tests
//         --gtest_filter="FullscreenControllerStateInteractiveTest.*"
//         --gtest_also_run_disabled_tests
//
// More context atop fullscreen_controller_state_test.h.
class FullscreenControllerStateInteractiveTest
    : public InProcessBrowserTest,
      public FullscreenControllerStateTest {
 public:
  FullscreenControllerStateInteractiveTest() = default;
  ~FullscreenControllerStateInteractiveTest() override = default;

  // InProcessBrowserTest:
  void TearDownOnMainThread() override {
    // This code needs to override TearDownOnMainThread() as that is called
    // before the Browser created by BrowserTestBase is deleted. TearDown() is
    // called after the browser has already been deleted, which means the test
    // code tries to remove an observer from a browser that was destroyed.
    FullscreenControllerStateTest::TearDown();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // FullscreenControllerStateTest:
  Browser* GetBrowser() override { return InProcessBrowserTest::browser(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerStateInteractiveTest);
};

// Soak tests ------------------------------------------------------------------

// Tests all states with all permutations of multiple events to detect lingering
// state issues that would bleed over to other states.
// I.E. for each state test all combinations of events E1, E2, E3.
//
// This produces coverage for event sequences that may happen normally but
// would not be exposed by traversing to each state via TransitionToState().
// TransitionToState() always takes the same path even when multiple paths
// exist.
IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest,
                       DISABLED_TransitionsForEachState) {
  // A tab is needed for tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  TestTransitionsForEachState();
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}


// Individual tests for each pair of state and event ---------------------------

// An "empty" test is included as part of each "TEST_EVENT" because it makes
// running the entire test suite less flaky on MacOS. All of the tests pass
// when run individually.
#if defined(OS_WIN)
#define TEST_EVENT(state, event)                                            \
  IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest,          \
                         state##__##event##__Empty) {}                      \
  IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest,          \
                         state##__##event) {                                \
    AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED); \
    ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event))                \
        << GetAndClearDebugLog();                                           \
  }
#else  // defined(OS_WIN)
#define TEST_EVENT(state, event)                                   \
  IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest, \
                         DISABLED_##state##__##event##__Empty) {}  \
  IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest, \
                         DISABLED_##state##__##event) {            \
    AddTabAtIndex(                                                 \
        0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);  \
    ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event))       \
        << GetAndClearDebugLog();                                  \
  }
#endif  // defined(OS_WIN)
        // Progress of tests can be examined by inserting the following line:
        // LOG(INFO) << GetAndClearDebugLog(); }

#include "chrome/browser/ui/exclusive_access/fullscreen_controller_state_tests.h"


// Specific one-off tests for known issues -------------------------------------

// Used manually to determine what happens on a platform.
IN_PROC_BROWSER_TEST_F(FullscreenControllerStateInteractiveTest,
                       DISABLED_ManualTest) {
  // A tab is needed for tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();

  // Wait, allowing human operator to observe the result.
  scoped_refptr<content::MessageLoopRunner> message_loop
      = new content::MessageLoopRunner();
  message_loop->Run();
}

