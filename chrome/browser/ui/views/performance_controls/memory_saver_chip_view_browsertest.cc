// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_browser_test_mixin.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_chip_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

class MemorySaverChipViewBrowserTest
    : public MemorySaverBrowserTestMixin<InProcessBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    MemorySaverBrowserTestMixin::SetUpOnMainThread();

    GURL test_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  PageActionIconView* GetMemorySaverChipView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kMemorySaver);
  }

  views::InkDropState GetInkDropState() {
    return views::InkDrop::Get(GetMemorySaverChipView())
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }
};

IN_PROC_BROWSER_TEST_F(MemorySaverChipViewBrowserTest,
                       ShowAndHideInkDropOnDialog) {
  PageActionIconView* chip = GetMemorySaverChipView();
  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(chip);

  EXPECT_TRUE(TryDiscardTabAt(0));
  chrome::SelectNumberedTab(browser(), 0);

  EXPECT_EQ(GetInkDropState(), views::InkDropState::HIDDEN);

  // Open bubble
  test_api.NotifyClick(press);

  EXPECT_EQ(GetInkDropState(), views::InkDropState::ACTIVATED);

  test_api.NotifyClick(press);

  views::InkDropState current_state = GetInkDropState();
  // The deactivated state is HIDDEN on Mac but DEACTIVATED on Linux.
  EXPECT_TRUE(current_state == views::InkDropState::HIDDEN ||
              current_state == views::InkDropState::DEACTIVATED);
}
