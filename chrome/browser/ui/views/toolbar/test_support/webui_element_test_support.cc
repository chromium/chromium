// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/test_support/webui_element_test_support.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

// static
WebUIToolbarWebView* WebUIElementTestSupport::GetToolbarWebView(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  return browser_view->toolbar_button_provider()
      ->GetWebUIToolbarViewForTesting();
}

// static
content::WebContents* WebUIElementTestSupport::GetToolbarWebContents(
    BrowserWindowInterface* browser) {
  WebUIToolbarWebView* const toolbar_view = GetToolbarWebView(browser);
  if (!toolbar_view || !toolbar_view->GetWebViewForTesting()) {
    return nullptr;
  }
  return toolbar_view->GetWebViewForTesting()->web_contents();
}

// static
ui::ElementContext WebUIElementTestSupport::GetElementContext(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return ui::ElementContext();
  }
  if (BrowserElements* const browser_elements =
          BrowserElements::From(browser)) {
    return browser_elements->GetContext();
  }
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view
             ? views::ElementTrackerViews::GetContextForView(browser_view)
             : ui::ElementContext();
}

// static
ui::TrackedElement* WebUIElementTestSupport::GetTrackedElement(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier element_id) {
  if (!browser || !element_id) {
    return nullptr;
  }
  if (BrowserElements* const browser_elements =
          BrowserElements::From(browser)) {
    return browser_elements->GetElement(element_id);
  }
  const ui::ElementContext context = GetElementContext(browser);
  return context
             ? ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                   element_id, context)
             : nullptr;
}

// static
bool WebUIElementTestSupport::IsElementVisible(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier element_id) {
  if (!element_id) {
    return false;
  }
  const ui::ElementContext context = GetElementContext(browser);
  return context && ui::ElementTracker::GetElementTracker()->IsElementVisible(
                        element_id, context);
}

// static
ui::TrackedElement* WebUIElementTestSupport::WaitForElementShown(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  const ui::ElementContext context = GetElementContext(browser);
  if (!context) {
    return nullptr;
  }
  ui::ElementTracker* const tracker = ui::ElementTracker::GetElementTracker();
  if (ui::TrackedElement* const existing =
          tracker->GetFirstMatchingElement(element_id, context)) {
    return existing;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  const auto subscription = tracker->AddElementShownCallback(
      element_id, context,
      base::IgnoreArgs<ui::TrackedElement*>(run_loop.QuitClosure()));
  run_loop.Run();
  return tracker->GetFirstMatchingElement(element_id, context);
}

// static
bool WebUIElementTestSupport::WaitForElementHidden(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier element_id) {
  if (!element_id) {
    return false;
  }
  const ui::ElementContext context = GetElementContext(browser);
  if (!context) {
    return false;
  }
  ui::ElementTracker* const tracker = ui::ElementTracker::GetElementTracker();
  if (!tracker->IsElementVisible(element_id, context)) {
    return true;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  const auto subscription = tracker->AddElementHiddenCallback(
      element_id, context,
      base::IgnoreArgs<ui::TrackedElement*>(run_loop.QuitClosure()));
  run_loop.Run();
  return !tracker->IsElementVisible(element_id, context);
}

// static
void WebUIElementTestSupport::PressTrackedElement(
    BrowserWindowInterface* browser,
    ui::TrackedElement* element,
    ui::test::InteractionTestUtil::InputType input_type) {
  ASSERT_TRUE(element);
  const bool is_webui = element->IsA<ui::TrackedElementWebUI>();
  ui::test::InteractionTestUtil test_util;
  test_util.AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
  test_util.AddSimulator(
      std::make_unique<ui::InteractionTestUtilSimulatorWebUI>());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.PressButton(element, input_type));
  if (is_webui) {
    EXPECT_TRUE(FlushToolbarPipes(browser));
  }
}

// static
::testing::AssertionResult WebUIElementTestSupport::FlushToolbarPipes(
    BrowserWindowInterface* browser) {
  WebUIToolbarWebView* const toolbar = GetToolbarWebView(browser);
  if (!toolbar) {
    return ::testing::AssertionFailure() << "no WebUI toolbar";
  }
  WebUIToolbarUI* const toolbar_ui = toolbar->GetWebUIToolbarUIForTesting();
  if (!toolbar_ui || !toolbar_ui->is_initialized()) {
    return ::testing::AssertionFailure() << "WebUI toolbar is not bound";
  }

  // Send any state push coalesced into a pending task before flushing Mojo.
  toolbar->FlushPendingToolbarState();
  toolbar_ui->FlushToolbarUIObserversForTesting();

  // Flush WebUI->browser via the page's BrowserProxy so both Mojo service
  // pipes used by the toolbar UI are round-tripped.
  static constexpr char kFlushJS[] = R"((async () => {
    const app = document.querySelector('toolbar-app');
    if (!app) {
      throw new Error('toolbar-app not found');
    }
    await Promise.all([
      app.browserProxyForTesting.flushBrowserControlsHandlerForTesting(),
      app.browserProxyForTesting.flushToolbarUiHandlerForTesting(),
    ]);
  })())";
  content::WebContents* const web_contents = GetToolbarWebContents(browser);
  if (!web_contents) {
    return ::testing::AssertionFailure() << "no WebUI toolbar WebContents";
  }
  return content::ExecJs(web_contents, kFlushJS);
}
