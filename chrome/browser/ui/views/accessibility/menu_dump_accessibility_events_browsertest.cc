// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view.h"

namespace views::test {

class DumpAccessibilityEventsMenuTest
    : public views::DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    auto container = std::make_unique<views::View>();
    container->SetLayoutManager(std::make_unique<views::FillLayout>());

    widget()->SetContentsView(std::move(container));
    widget()->Show();
    views::test::WaitForWidgetActive(widget(), true);
  }

  void TearDownOnMainThread() override {
    menu_runner_.reset();
    menu_delegate_.reset();
    views::DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

  // Strip group position properties (SetSize, PosInSet, Level) from event
  // lines so that ViewsAXEnabled and ViewsAXDisabled produce identical output.
  // The ViewsAXEnabled path serializes group position through the AXTree, while
  // the ViewsAXDisabled path does not compute it for menu views.
  std::vector<std::string> CollectEventLogs() override {
    auto logs = views::DumpAccessibilityEventsViewsTestBase::CollectEventLogs();
    for (auto& log : logs) {
      StripGroupPositionProperty(log, " SetSize=");
      StripGroupPositionProperty(log, " PosInSet=");
      StripGroupPositionProperty(log, " Level=");
#if BUILDFLAG(IS_MAC)
      // Generated Mac MENU_POPUP_END events are intentionally retargeted to
      // the tree root, while the legacy ViewsAX-disabled source event fires on
      // the menu. This test only cares that a close event is emitted.
      if (log.starts_with("AXMenuClosed ")) {
        log = "AXMenuClosed";
      }
#endif
    }
    return logs;
  }

 protected:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters =
        views::DumpAccessibilityEventsViewsTestBase::DefaultFilters();

    // Scope this down to menu events, since that's what this test verifies.
    filters.emplace_back("*", ui::AXPropertyFilter::DENY);

#if BUILDFLAG(IS_WIN)
    filters.emplace_back("EVENT_SYSTEM_MENU*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("Menu*", ui::AXPropertyFilter::ALLOW);
#elif BUILDFLAG(IS_MAC)
    filters.emplace_back("AXMenuOpened*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("AXMenuClosed*", ui::AXPropertyFilter::ALLOW);
#elif BUILDFLAG(IS_LINUX)
    filters.emplace_back("STATE-CHANGE:SHOWING:TRUE*ROLE_MENU*",
                         ui::AXPropertyFilter::ALLOW);
#endif

    return filters;
  }

  void WaitForSubmenuSerialization(views::SubmenuView* submenu) {
    if (!IsViewsAXEnabled() || !submenu || !submenu->GetWidget() ||
        !submenu->GetWidget()->ax_manager()) {
      return;
    }

    views::WidgetAXManagerTestApi test_api(submenu->GetWidget()->ax_manager());
    if (test_api.processing_update_posted()) {
      test_api.WaitForNextSerialization();
    }
  }

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::MenuDelegate> menu_delegate_;

 private:
  static void StripGroupPositionProperty(std::string& str,
                                         std::string_view prefix) {
    size_t pos = str.find(prefix);
    if (pos == std::string::npos) {
      return;
    }
    size_t end = pos + prefix.size();
    while (end < str.size() && std::isdigit(str[end])) {
      ++end;
    }
    str.erase(pos, end - pos);
  }
};

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsMenuTest, MenuShowHide) {
#if BUILDFLAG(IS_WIN)
  if (GetApiType() == ui::AXApiType::kWinUIA) {
    // TODO(crbug.com/40672441): Re-enable once UIA supports PID-based event
    // capture.
    GTEST_SKIP() << "UIA menu dump tests need PID-based event capture.";
  }
#endif

  BEGIN_RECORDING_EVENTS_OR_SKIP("menu-show-hide");

  menu_delegate_ = std::make_unique<views::MenuDelegate>();
  auto menu_item_owning =
      std::make_unique<views::MenuItemView>(menu_delegate_.get());
  views::MenuItemView* menu_item = menu_item_owning.get();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(menu_item_owning), views::MenuRunner::CONTEXT_MENU);
  menu_item->AppendMenuItem(1, u"Item One");
  menu_item->AppendMenuItem(2, u"Item Two");
  views::SubmenuView* submenu = menu_item->GetSubmenu();

  menu_runner_->RunMenuAt(widget(), nullptr, gfx::Rect(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kNone);
  WaitForPendingSerialization();
  WaitForSubmenuSerialization(submenu);

  menu_runner_->Cancel();
  WaitForPendingSerialization();
  WaitForSubmenuSerialization(submenu);
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsMenuTest, MenuNested) {
#if BUILDFLAG(IS_WIN)
  if (GetApiType() == ui::AXApiType::kWinUIA) {
    // TODO(crbug.com/40672441): Re-enable once UIA supports PID-based event
    // capture.
    GTEST_SKIP() << "UIA menu dump tests need PID-based event capture.";
  }
#endif

  BEGIN_RECORDING_EVENTS_OR_SKIP("menu-nested");

  menu_delegate_ = std::make_unique<views::MenuDelegate>();
  auto menu_item_owning =
      std::make_unique<views::MenuItemView>(menu_delegate_.get());
  views::MenuItemView* root_item = menu_item_owning.get();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(menu_item_owning), views::MenuRunner::CONTEXT_MENU);

  root_item->AppendMenuItem(1, u"Root Item");
  views::MenuItemView* level2_item =
      root_item->AppendSubMenu(2, u"Level 2 Submenu");
  level2_item->AppendMenuItem(3, u"Level 2 Item");
  views::MenuItemView* level3_item =
      level2_item->AppendSubMenu(4, u"Level 3 Submenu");
  level3_item->AppendMenuItem(5, u"Level 3 Item A");
  level3_item->AppendMenuItem(6, u"Level 3 Item B");

  menu_runner_->RunMenuAt(widget(), nullptr, gfx::Rect(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kNone);
  WaitForPendingSerialization();
  views::SubmenuView* root_submenu = root_item->GetSubmenu();
  WaitForSubmenuSerialization(root_submenu);

  views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
      level2_item);
  WaitForPendingSerialization();
  views::SubmenuView* level2_submenu = level2_item->GetSubmenu();
  WaitForSubmenuSerialization(level2_submenu);

  views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
      level3_item);
  WaitForPendingSerialization();
  views::SubmenuView* level3_submenu = level3_item->GetSubmenu();
  WaitForSubmenuSerialization(level3_submenu);
#if BUILDFLAG(IS_LINUX)
  ASSERT_TRUE(WaitForCapturedEvent(
      "STATE-CHANGE:SHOWING:TRUE role=ROLE_MENU name='Level 3 Submenu'"));
#endif

  menu_runner_->Cancel();
  WaitForPendingSerialization();
  WaitForSubmenuSerialization(level3_submenu);
  WaitForSubmenuSerialization(level2_submenu);
  WaitForSubmenuSerialization(root_submenu);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsMenuTest,
    ::testing::ValuesIn(
        views::DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    views::EventTestPassToString());

}  // namespace views::test
