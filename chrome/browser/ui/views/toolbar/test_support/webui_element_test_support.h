// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_WEBUI_ELEMENT_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_WEBUI_ELEMENT_TEST_SUPPORT_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"

class BrowserWindowInterface;
class WebUIToolbarWebView;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class TrackedElement;
}  // namespace ui

// Shared helper for locating, waiting on, and interacting with WebUI toolbar
// elements in `TestAccessor`s.
class WebUIElementTestSupport {
 public:
  // Returns the `WebUIToolbarWebView` or its `WebContents` for `browser`, or
  // `nullptr` when the WebUI toolbar is not present.
  static WebUIToolbarWebView* GetToolbarWebView(
      BrowserWindowInterface* browser);
  static content::WebContents* GetToolbarWebContents(
      BrowserWindowInterface* browser);

  // Queries or waits on `ElementTracker` state for `element_id` in `browser`.
  static ui::ElementContext GetElementContext(BrowserWindowInterface* browser);
  static ui::TrackedElement* GetTrackedElement(
      BrowserWindowInterface* browser,
      ui::ElementIdentifier element_id);
  static bool IsElementVisible(BrowserWindowInterface* browser,
                               ui::ElementIdentifier element_id);
  static ui::TrackedElement* WaitForElementShown(
      BrowserWindowInterface* browser,
      ui::ElementIdentifier element_id);
  [[nodiscard]] static bool WaitForElementHidden(
      BrowserWindowInterface* browser,
      ui::ElementIdentifier element_id);

  // Presses `element` (Views or WebUI) and flushes WebUI toolbar Mojo pipes
  // when `element` is backed by `ui::TrackedElementWebUI`.
  static void PressTrackedElement(
      BrowserWindowInterface* browser,
      ui::TrackedElement* element,
      ui::test::InteractionTestUtil::InputType input_type =
          ui::test::InteractionTestUtil::InputType::kMouse);

  // Flushes pending C++->WebUI state pushes and WebUI->C++ service messages.
  // Use only when there is no observable state change to wait for instead.
  [[nodiscard]] static ::testing::AssertionResult FlushToolbarPipes(
      BrowserWindowInterface* browser);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_WEBUI_ELEMENT_TEST_SUPPORT_H_
