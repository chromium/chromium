// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/platform/assistive_tech.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class TabStripDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters;
    filters.emplace_back("EVENT_OBJECT_SELECTION*",
                         ui::AXPropertyFilter::ALLOW);
    return filters;
  }

  gfx::NativeWindow GetTargetNativeWindow() const override {
    return GetBrowserWidget()->GetNativeWindow();
  }

  View* GetTargetRootView() const override {
    return GetBrowserWidget()->GetRootView();
  }

  // Don't create fake views; use the real browser's view hierarchy.
  // Show the base class widget so it can be used for deactivation.
  void SetUpTestViews() override { widget()->Show(); }

 protected:
  Widget* GetBrowserWidget() const {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  }

  TabStrip* GetTabStrip() const {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->horizontal_tab_strip_for_testing();
  }

  void WaitForBrowserSerialization() {
    if (!IsViewsAXEnabled()) {
      return;
    }
    Widget* bw = GetBrowserWidget();
    if (!bw || !bw->ax_manager()) {
      return;
    }
    WidgetAXManagerTestApi test_api(bw->ax_manager());
    if (test_api.processing_update_posted()) {
      test_api.WaitForNextSerialization();
    }
  }
};

IN_PROC_BROWSER_TEST_P(TabStripDumpAccessibilityEventsTest,
                       BrowserRootViewClassNameGuard) {
  views::View* root = GetBrowserWidget()->GetRootView();
  // If this fails, `BrowserRootView` has been renamed and the constant in
  // `BrowserAccessibilityManagerWin` that relies on this classname must be
  // updated.
  ASSERT_EQ(root->GetClassName(), "BrowserRootView");
}

IN_PROC_BROWSER_TEST_P(TabStripDumpAccessibilityEventsTest,
                       WindowActivationFiresSelectionForJaws) {
  ui::AXPlatform::GetInstance().NotifyAssistiveTechChanged(
      ui::AssistiveTech::kJaws);

  WaitForBrowserSerialization();

  // Deactivate the browser by activating the test widget.
  widget()->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !GetBrowserWidget()->IsActive(); }));
  WaitForBrowserSerialization();

  BEGIN_RECORDING_EVENTS_OR_SKIP(
      "tab-window-activation-fires-selection-for-jaws");

  // Reactivate the browser. This should fire the selection event for JAWS.
  GetBrowserWidget()->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return GetBrowserWidget()->IsActive(); }));
  WaitForBrowserSerialization();
}

IN_PROC_BROWSER_TEST_P(TabStripDumpAccessibilityEventsTest,
                       WindowActivationNoSelectionWithoutJaws) {
  SKIP_IF_VIEWS_AX_DISABLED();

  ui::AXPlatform::GetInstance().NotifyAssistiveTechChanged(
      ui::AssistiveTech::kNone);

  WaitForBrowserSerialization();

  BEGIN_RECORDING_EVENTS_OR_SKIP(
      "tab-window-activation-no-selection-without-jaws");

  GetBrowserWidget()->GetRootView()->GetViewAccessibility().NotifyEvent(
      ax::mojom::Event::kWindowActivated, true);
  WaitForBrowserSerialization();
}

IN_PROC_BROWSER_TEST_P(TabStripDumpAccessibilityEventsTest,
                       WindowActivationFiresSelectionOnNewTab) {
  ui::AXPlatform::GetInstance().NotifyAssistiveTechChanged(
      ui::AssistiveTech::kJaws);

  chrome::NewTab(browser());
  WaitForBrowserSerialization();

  widget()->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !GetBrowserWidget()->IsActive(); }));
  WaitForBrowserSerialization();

  BEGIN_RECORDING_EVENTS_OR_SKIP(
      "tab-window-activation-fires-selection-on-new-tab");

  GetBrowserWidget()->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return GetBrowserWidget()->IsActive(); }));
  WaitForBrowserSerialization();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabStripDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
