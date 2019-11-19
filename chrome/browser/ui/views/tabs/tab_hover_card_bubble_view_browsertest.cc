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
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using views::Widget;

// Helper to wait until a window is deactivated.
class WindowDeactivedWaiter : public views::WidgetObserver {
 public:
  explicit WindowDeactivedWaiter(BrowserView* window) : window_(window) {
    window_->frame()->AddObserver(this);
  }
  ~WindowDeactivedWaiter() override { window_->frame()->RemoveObserver(this); }

  void Wait() {
    if (!window_->IsActive())
      return;
    run_loop_.Run();
  }

  // WidgetObserver overrides:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (!active)
      run_loop_.Quit();
  }

 private:
  BrowserView* const window_;
  base::RunLoop run_loop_;
};

// Helper to wait until the hover card widget is visible.
class HoverCardVisibleWaiter : public views::WidgetObserver {
 public:
  explicit HoverCardVisibleWaiter(Widget* hover_card)
      : hover_card_(hover_card) {
    hover_card_->AddObserver(this);
  }
  ~HoverCardVisibleWaiter() override { hover_card_->RemoveObserver(this); }

  void Wait() {
    if (hover_card_->IsVisible())
      return;
    run_loop_.Run();
  }

  // WidgetObserver overrides:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    if (visible)
      run_loop_.Quit();
  }

 private:
  Widget* const hover_card_;
  base::RunLoop run_loop_;
};

class TabHoverCardBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  TabHoverCardBubbleViewBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    TabHoverCardBubbleView::disable_animations_for_testing_ = true;
  }
  ~TabHoverCardBubbleViewBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCards);
    DialogBrowserTest::SetUp();
  }

  static TabHoverCardBubbleView* GetHoverCard(const TabStrip* tabstrip) {
    return tabstrip->hover_card_;
  }

  static Widget* GetHoverCardWidget(const TabHoverCardBubbleView* hover_card) {
    return hover_card->widget_;
  }

  const base::string16& GetHoverCardTitle(
      const TabHoverCardBubbleView* hover_card) {
    return hover_card->title_label_->GetText();
  }

  const base::string16& GetHoverCardDomain(
      const TabHoverCardBubbleView* hover_card) {
    return hover_card->domain_label_->GetText();
  }

  void MouseExitTabStrip() {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    ui::MouseEvent stop_hover_event(ui::ET_MOUSE_EXITED, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_NONE, 0);
    tab_strip->OnMouseExited(stop_hover_event);
  }

  void ClickMouseOnTab(int index) {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMousePressed(click_event);
  }

  void HoverMouseOverTabAt(int index) {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(index);
    ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMouseEntered(hover_event);
  }

  int GetHoverCardsSeenCount(const TabHoverCardBubbleView* hover_card) {
    return hover_card->hover_cards_seen_count_;
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    Tab* tab = tab_strip->tab_at(0);
    ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
    tab->OnMouseEntered(hover_event);
    TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
    Widget* widget = GetHoverCardWidget(hover_card);
    HoverCardVisibleWaiter waiter(widget);
    waiter.Wait();
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TabHoverCardBubbleViewBrowserTest);
};

// Fails on win: http://crbug.com/932402.
#if defined(OS_WIN)
#define MAYBE_InvokeUi_tab_hover_card DISABLED_InvokeUi_tab_hover_card
#else
#define MAYBE_InvokeUi_tab_hover_card InvokeUi_tab_hover_card
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_InvokeUi_tab_hover_card) {
  ShowAndVerifyUi();
}

// Verify hover card is visible while hovering and not visible outside of the
// tabstrip.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnHover) {
  ShowUi("default");
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  MouseExitTabStrip();
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is visible when tab is focused.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnTabFocus) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is visible when focus moves from the tab to tab close
// button.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnTabCloseButtonFocusAfterTabFocus) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  tab_strip->GetFocusManager()->SetFocusedView(tab->close_button_);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is visible when tab is focused and a key is pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnKeyPressAfterTabFocus) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
  tab->OnKeyPressed(key_event);
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is not visible when tab is focused and the mouse is
// pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetNotVisibleOnMousePressAfterTabFocus) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  ClickMouseOnTab(0);
  EXPECT_FALSE(widget->IsVisible());
}

// Verify hover card is visible after navigating to the tab strip using keyboard
// accelerators.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetVisibleOnTabFocusFromKeyboardAccelerator) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = base::UTF8ToUTF16("Test Tab 2");
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip->AddTabAt(1, new_tab_data, false);

  // Cycle focus until it reaches a tab.
  while (!tab_strip->IsFocusInTabs())
    browser()->command_controller()->ExecuteCommand(IDC_FOCUS_NEXT_PANE);

  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  // Move focus forward to the close button or next tab dependent on window
  // size.
  tab_strip->AcceleratorPressed(ui::Accelerator(ui::VKEY_RIGHT, ui::EF_NONE));
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
}

// Verify hover card is not visible after clicking on a tab.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       WidgetNotVisibleOnClick) {
  ShowUi("default");
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  ClickMouseOnTab(0);
  EXPECT_FALSE(widget->IsVisible());
}

// Verify title, domain, and anchor are correctly updated when moving hover
// from one tab to another.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest, WidgetDataUpdate) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = base::UTF8ToUTF16("Test Tab 2");
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip->AddTabAt(1, new_tab_data, false);

  ShowUi("default");
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  HoverMouseOverTabAt(1);
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());
  Tab* tab = tab_strip->tab_at(1);
  EXPECT_EQ(GetHoverCardTitle(hover_card), base::UTF8ToUTF16("Test Tab 2"));
  EXPECT_EQ(GetHoverCardDomain(hover_card), base::UTF8ToUTF16("example.com"));
  EXPECT_EQ(hover_card->GetAnchorView(), static_cast<views::View*>(tab));
}

// Verify inactive window remains inactive when showing a hover card for a tab
// in the inactive window.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       InactiveWindowStaysInactiveOnHover) {
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
  WindowDeactivedWaiter waiter(
      BrowserView::GetBrowserViewForBrowser(inactive_window));
  BrowserView::GetBrowserViewForBrowser(active_window)->Activate();
  waiter.Wait();
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(inactive_window)->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, 0);
  tab->OnMouseEntered(hover_event);
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
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  tab_strip->AddTabAt(1, TabRendererData(), false);
  tab_strip->AddTabAt(2, TabRendererData(), false);

  HoverMouseOverTabAt(0);

  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();

  EXPECT_EQ(GetHoverCardsSeenCount(hover_card), 1);
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  HoverMouseOverTabAt(1);
  EXPECT_EQ(GetHoverCardsSeenCount(hover_card), 2);
  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(1);
  tab_strip->SetSelection(selection);
  EXPECT_EQ(GetHoverCardsSeenCount(hover_card), 0);
  EXPECT_FALSE(widget->IsVisible());
}
