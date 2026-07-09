// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class BrowserAppMenuButtonDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  gfx::NativeWindow GetTargetNativeWindow() const override {
    return GetBrowserWidget()->GetNativeWindow();
  }

  View* GetTargetRootView() const override {
    return GetBrowserWidget()->GetRootView();
  }

  // Use the real browser's view hierarchy rather than synthetic views. The base
  // class widget is still shown so it can be used for teardown.
  void SetUpTestViews() override { widget()->Show(); }

 protected:
  Widget* GetBrowserWidget() const {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  }

  BrowserAppMenuButton* GetAppMenuButton() const {
    return AsViewClass<BrowserAppMenuButton>(
        ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kToolbarAppMenuButtonElementId,
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetElementContext()));
  }

  void WaitForBrowserSerialization() {
    if (!IsViewsAXEnabled()) {
      return;
    }
    Widget* browser_widget = GetBrowserWidget();
    if (!browser_widget || !browser_widget->ax_manager()) {
      return;
    }
    WidgetAXManagerTestApi test_api(browser_widget->ax_manager());
    if (test_api.processing_update_posted()) {
      test_api.WaitForNextSerialization();
    }
  }
};

IN_PROC_BROWSER_TEST_P(BrowserAppMenuButtonDumpAccessibilityEventsTest,
                       AppMenuButtonExpandCollapseStateChanged) {
  // The legacy non-ViewsAX path has a known bug that raises ToggleState here;
  // enabling ViewsAX is what fixes it.
  SKIP_IF_VIEWS_AX_DISABLED();

  SetFilters(R"(
@UIA-WIN-ALLOW:ExpandCollapseExpandCollapseState*
@UIA-WIN-ALLOW:ToggleToggleState*
)");

  // Flush any accessibility events generated while the browser was created so
  // only the checked-state change below is recorded.
  WaitForBrowserSerialization();

  BEGIN_RECORDING_EVENTS_OR_SKIP("browser-app-menu-button-expand-collapse");

  // Opening the app menu keeps the button pressed via a MenuButtonController
  // lock, and a pressed button reports an accessible checked state of true.
  // Take that same lock here to fire the checked-state change without spinning
  // a nested menu run loop.
  std::unique_ptr<MenuButtonController::PressedLock> pressed_lock =
      GetAppMenuButton()->menu_button_controller()->TakeLock();

  ASSERT_TRUE(WaitForCapturedEvent(
      "ExpandCollapseExpandCollapseState changed on role=button"));
  event_recording_session_.StopAndCompare();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAppMenuButtonDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
