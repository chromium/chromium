// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using views::Widget;

class TabHoverCardBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  TabHoverCardBubbleViewBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    TabHoverCardBubbleView::disable_animations_for_testing_ = true;
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCards);
  }
  TabHoverCardBubbleViewBrowserTest(const TabHoverCardBubbleViewBrowserTest&) =
      delete;
  TabHoverCardBubbleViewBrowserTest& operator=(
      const TabHoverCardBubbleViewBrowserTest&) = delete;
  ~TabHoverCardBubbleViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    tab_strip_ = BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  TabStrip* tab_strip() { return tab_strip_; }

  TabHoverCardBubbleView* hover_card() { return tab_strip()->hover_card_; }

  base::string16 GetHoverCardTitle() {
    return hover_card()->title_label_->GetText();
  }

  base::string16 GetHoverCardDomain() {
    return hover_card()->domain_label_->GetText();
  }

  int GetHoverCardsSeenCount() { return hover_card()->hover_cards_seen_count_; }

  void MouseExitTabStrip() {
    ui::MouseEvent stop_hover_event(ui::ET_MOUSE_EXITED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_NONE, 0);
    tab_strip()->OnMouseExited(stop_hover_event);
  }

  void ClickMouseOnTab(int index) {
    Tab* const tab = tab_strip()->tab_at(index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMousePressed(click_event);
  }

  void HoverMouseOverTabAt(int index) {
    // We don't use Tab::OnMouseEntered here to invoke the hover card because
    // that path is disabled in browser tests. If we enabled it, the real mouse
    // might interfere with the test.
    tab_strip()->UpdateHoverCard(tab_strip()->tab_at(index));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    HoverMouseOverTabAt(0);
    views::test::WidgetVisibleWaiter(hover_card()->GetWidget()).Wait();
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  base::test::ScopedFeatureList scoped_feature_list_;

  TabStrip* tab_strip_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       InvokeUi_tab_hover_card) {
  ShowAndVerifyUi();
}

// Verify hover card is visible while hovering and not visible outside of the
// tabstrip.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_WIN)
#define MAYBE_WidgetVisibleOnHover DISABLED_WidgetVisibleOnHover
#else
#define MAYBE_WidgetVisibleOnHover WidgetVisibleOnHover
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetVisibleOnHover) {
  ShowUi("default");
  Widget* const widget = hover_card()->GetWidget();

  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  MouseExitTabStrip();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is visible when tab is focused.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_WidgetVisibleOnTabFocus DISABLED_WidgetVisibleOnTabFocus
#else
#define MAYBE_WidgetVisibleOnTabFocus WidgetVisibleOnTabFocus
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetVisibleOnTabFocus) {
  Tab* const tab = tab_strip()->tab_at(0);
  tab_strip()->GetFocusManager()->SetFocusedView(tab);
  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is visible when focus moves from the tab to tab close
// button.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_WidgetVisibleOnTabCloseButtonFocusAfterTabFocus \
  DISABLED_WidgetVisibleOnTabCloseButtonFocusAfterTabFocus
#else
#define MAYBE_WidgetVisibleOnTabCloseButtonFocusAfterTabFocus \
  WidgetVisibleOnTabCloseButtonFocusAfterTabFocus
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetVisibleOnTabCloseButtonFocusAfterTabFocus) {
  Tab* const tab = tab_strip()->tab_at(0);
  tab_strip()->GetFocusManager()->SetFocusedView(tab);
  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());
  tab_strip()->GetFocusManager()->SetFocusedView(tab->close_button_);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is visible when tab is focused and a key is pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnKeyPressAfterTabFocus) {
  Tab* const tab = tab_strip()->tab_at(0);
  tab_strip()->GetFocusManager()->SetFocusedView(tab);
  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
  tab->OnKeyPressed(key_event);
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is not visible when tab is focused and the mouse is
// pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetNotVisibleOnMousePressAfterTabFocus) {
  Tab* const tab = tab_strip()->tab_at(0);
  tab_strip()->GetFocusManager()->SetFocusedView(tab);
  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  ClickMouseOnTab(0);
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is visible after navigating to the tab strip using keyboard
// accelerators.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator \
  DISABLED_WidgetVisibleOnTabFocusFromKeyboardAccelerator
#else
#define MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator \
  WidgetVisibleOnTabFocusFromKeyboardAccelerator
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator) {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = base::UTF8ToUTF16("Test Tab 2");
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip()->AddTabAt(1, new_tab_data, false);

  // Cycle focus until it reaches a tab.
  while (!tab_strip()->IsFocusInTabs())
    browser()->command_controller()->ExecuteCommand(IDC_FOCUS_NEXT_PANE);

  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  // Move focus forward to the close button or next tab dependent on window
  // size.
  tab_strip()->AcceleratorPressed(ui::Accelerator(ui::VKEY_RIGHT, ui::EF_NONE));
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is not visible after clicking on a tab.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_WIN)
#define MAYBE_WidgetNotVisibleOnClick DISABLED_WidgetNotVisibleOnClick
#else
#define MAYBE_WidgetNotVisibleOnClick WidgetNotVisibleOnClick
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetNotVisibleOnClick) {
  ShowUi("default");
  Widget* const widget = hover_card()->GetWidget();

  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  ClickMouseOnTab(0);
  EXPECT_FALSE(widget->IsVisible());
}

// Verify title, domain, and anchor are correctly updated when moving hover
// from one tab to another.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_WIN)
#define MAYBE_WidgetDataUpdate DISABLED_WidgetDataUpdate
#else
#define MAYBE_WidgetDataUpdate WidgetDataUpdate
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetDataUpdate) {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = base::UTF8ToUTF16("Test Tab 2");
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip()->AddTabAt(1, new_tab_data, false);

  ShowUi("default");

  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  HoverMouseOverTabAt(1);
  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  Tab* const tab = tab_strip()->tab_at(1);
  EXPECT_EQ(GetHoverCardTitle(), base::ASCIIToUTF16("Test Tab 2"));
  EXPECT_EQ(GetHoverCardDomain(), base::ASCIIToUTF16("example.com"));
  EXPECT_EQ(hover_card()->GetAnchorView(), static_cast<views::View*>(tab));
}

// Verify inactive window remains inactive when showing a hover card for a tab
// in the inactive window.
// TODO(crbug.com/1050765): the test is flaky.
#if defined(OS_WIN)
#define MAYBE_InactiveWindowStaysInactiveOnHover \
  DISABLED_InactiveWindowStaysInactiveOnHover
#else
#define MAYBE_InactiveWindowStaysInactiveOnHover \
  InactiveWindowStaysInactiveOnHover
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_InactiveWindowStaysInactiveOnHover) {
  const BrowserList* active_browser_list_ = BrowserList::GetInstance();
  // Open a second window. On Windows, Widget::Deactivate() works by activating
  // the next topmost window on the z-order stack. So instead we use two windows
  // and Widget::Activate() here to prevent Widget::Deactivate() from
  // undesirably activating another window when tests are being run in parallel.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ShowUi("default");
  ASSERT_EQ(2u, active_browser_list_->size());
  Browser* active_window = active_browser_list_->get(0);
  Browser* inactive_window = active_browser_list_->get(1);
  views::test::WidgetActivationWaiter waiter(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->frame(), false);
  BrowserView::GetBrowserViewForBrowser(active_window)->Activate();
  waiter.Wait();
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
  HoverMouseOverTabAt(0);
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
}

// Verify counter for tab hover cards seen ratio metric increases as hover
// cards are shown and is reset when a tab is selected.
// Fails on Windows, see crbug.com/990210.
#if defined(OS_WIN)
#define MAYBE_HoverCardsSeenRatioMetric DISABLED_HoverCardsSeenRatioMetric
#else
#define MAYBE_HoverCardsSeenRatioMetric HoverCardsSeenRatioMetric
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_HoverCardsSeenRatioMetric) {
  tab_strip()->AddTabAt(1, TabRendererData(), false);
  tab_strip()->AddTabAt(2, TabRendererData(), false);

  HoverMouseOverTabAt(0);

  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();

  EXPECT_EQ(GetHoverCardsSeenCount(), 1);
  EXPECT_TRUE(widget->IsVisible());

  HoverMouseOverTabAt(1);
  EXPECT_EQ(GetHoverCardsSeenCount(), 2);
  EXPECT_TRUE(widget->IsVisible());

  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(1);
  tab_strip()->SetSelection(selection);
  EXPECT_EQ(GetHoverCardsSeenCount(), 0);
  EXPECT_FALSE(widget->IsVisible());
}
