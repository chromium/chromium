// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTION_TEST_UTIL_BROWSER_H_
#define CHROME_TEST_INTERACTION_INTERACTION_TEST_UTIL_BROWSER_H_

#include <string>

#include "ui/base/interaction/interaction_test_util.h"

namespace ui {
class TrackedElement;
}  // namespace ui

class Browser;

class InteractionTestUtilBrowser : public ui::test::InteractionTestUtil {
 public:
  InteractionTestUtilBrowser();
  ~InteractionTestUtilBrowser() override;

  // Returns the browser that matches the given context, or nullptr if none
  // can be found.
  static Browser* GetBrowserFromContext(ui::ElementContext context);

  // Takes a screenshot based on the contents of `element` and compares with
  // Skia Gold. May return ActionResult::kKnownIncompatible on platforms and
  // builds where screenshots are not supported or not reliable. This is not
  // necessarily an error; the remainder of the test may still be valid.
  //
  // The name of the screenshot will be composed as follows:
  //   TestFixture_TestName[_screenshot_name]_baseline
  // If you are taking more than one screenshot per test, then `screenshot_name`
  // must be specified and unique within the test; otherwise you may leave it
  // empty.
  //
  // IMPORTANT USAGE NOTES:
  //
  // Element must be on a surface that is visible and not occluded (for example,
  // a widget, or the active tab in a browser).
  //
  // If `element` is a TrackedElementWebContents of any sort, it is useful to
  // verify that the contents you intend to take a screenshot of are present and
  // rendered before taking the screenshot. One way to do this is by calling:
  //  - SendEventOnElementMinimumSize() for pages in browser tabs
  //  - SendEventOnWebViewMinimumSize() for secondary WebUI
  // These are especially important if your WebView contains any dynamic content
  // that may populate and display after the page is loaded. After you receive
  // the event, you should be able to call CompareScreenshot() safely.
  //
  // In order to actually take screenshots:
  // - Your test must be in browser_tests or interactive_ui_tests
  // - Your test must be included in pixel_tests.filter
  //
  // Note that test in browser_tests may run at the same time as other tests,
  // which can result in flakiness (especially if mouse position, window
  // activation, or occlusion could change the behavior of a test). So if you
  // need to both test complex interaction and take screenshots, prefer putting
  // your test in interactive_ui_tests.
  static ui::test::ActionResult CompareScreenshot(
      ui::TrackedElement* element,
      const std::string& screenshot_name,
      const std::string& baseline);
};

#endif  // CHROME_TEST_INTERACTION_INTERACTION_TEST_UTIL_BROWSER_H_
