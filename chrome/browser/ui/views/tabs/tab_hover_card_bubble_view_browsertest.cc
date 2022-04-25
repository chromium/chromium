// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_metrics.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
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
    TabHoverCardController::disable_animations_for_testing_ = true;
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

  TabHoverCardBubbleView* hover_card() {
    return tab_strip()->hover_card_controller_->hover_card_;
  }

  std::u16string GetHoverCardTitle() {
    return hover_card()->GetTitleTextForTesting();
  }

  std::u16string GetHoverCardDomain() {
    return hover_card()->GetDomainTextForTesting();
  }

  int GetHoverCardsSeenCount() {
    return tab_strip()
        ->hover_card_controller_->metrics_for_testing()
        ->cards_seen_count();
  }

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
    tab_strip()->UpdateHoverCard(
        tab_strip()->tab_at(index),
        TabSlotController::HoverCardUpdateType::kHover);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    HoverMouseOverTabAt(0);
    views::test::WidgetVisibleWaiter(hover_card()->GetWidget()).Wait();
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  raw_ptr<TabStrip> tab_strip_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       InvokeUi_tab_hover_card) {
  ShowAndVerifyUi();
}

// Verify hover card is visible while hovering and not visible outside of the
// tabstrip.
// TODO(crbug.com/1050765): the test is flaky.
#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator \
  DISABLED_WidgetVisibleOnTabFocusFromKeyboardAccelerator
#else
#define MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator \
  WidgetVisibleOnTabFocusFromKeyboardAccelerator
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetVisibleOnTabFocusFromKeyboardAccelerator) {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = u"Test Tab 2";
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip()->AddTabAt(1, new_tab_data);

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
#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_WIN)
#define MAYBE_WidgetDataUpdate DISABLED_WidgetDataUpdate
#else
#define MAYBE_WidgetDataUpdate WidgetDataUpdate
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       MAYBE_WidgetDataUpdate) {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = u"Test Tab 2";
  new_tab_data.last_committed_url =
      GURL("http://example.com/this/should/not/be/seen");
  tab_strip()->AddTabAt(1, new_tab_data);

  ShowUi("default");

  Widget* const widget = hover_card()->GetWidget();
  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  HoverMouseOverTabAt(1);
  ASSERT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
  Tab* const tab = tab_strip()->tab_at(1);
  EXPECT_EQ(GetHoverCardTitle(), u"Test Tab 2");
  EXPECT_EQ(GetHoverCardDomain(), u"example.com");
  EXPECT_EQ(hover_card()->GetAnchorView(), static_cast<views::View*>(tab));
}

// Verify inactive window remains inactive when showing a hover card for a tab
// in the inactive window.
// TODO(crbug.com/1050765): the test is flaky.
#if BUILDFLAG(IS_WIN)
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

IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewBrowserTest,
                       HoverCardsSeenRatioMetric) {
  tab_strip()->AddTabAt(1, TabRendererData());
  tab_strip()->AddTabAt(2, TabRendererData());

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

// Tests for tabs showing interstitials to check whether the URL in the hover
// card is displayed or hidden as appropriate.
class TabHoverCardBubbleViewInterstitialBrowserTest
    : public TabHoverCardBubbleViewBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_mismatched_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_mismatched_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_->AddDefaultHandlers(GetChromeTestDataDir());

    TabHoverCardBubbleViewBrowserTest::SetUpOnMainThread();
    reputation::InitializeSafetyTipConfig();
  }

  net::EmbeddedTestServer* https_server_mismatched() {
    return https_server_mismatched_.get();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_mismatched_;
};

// Verify that the domain field of tab's hover card is empty if the tab is
// showing a lookalike interstitial is ("Did you mean google.com?").
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInterstitialBrowserTest,
                       LookalikeInterstitial_ShouldHideHoverCardUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate the tab to a lookalike URL and check the hover card. The domain
  // field must be empty.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("googlé.com", "/empty.html")));
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Open another tab.
  chrome::NewTab(browser());
  HoverMouseOverTabAt(0);
  views::test::WidgetVisibleWaiter(hover_card()->GetWidget()).Wait();

  EXPECT_TRUE(GetHoverCardDomain().empty());
  EXPECT_EQ(GetHoverCardsSeenCount(), 1);
  ASSERT_NE(nullptr, hover_card()->GetWidget());
  EXPECT_TRUE(hover_card()->GetWidget()->IsVisible());
}

// Verify that the domain field of tab's hover card is not empty on other types
// of interstitials (here, SSL).
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInterstitialBrowserTest,
                       SSLInterstitial_ShouldShowHoverCardUrl) {
  ASSERT_TRUE(https_server_mismatched()->Start());
  // Navigate the tab to an SSL error.
  const GURL url =
      https_server_mismatched()->GetURL("site.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open another tab.
  chrome::NewTab(browser());
  HoverMouseOverTabAt(0);
  views::test::WidgetVisibleWaiter(hover_card()->GetWidget()).Wait();

  EXPECT_EQ(base::UTF8ToUTF16(net::GetHostAndPort(url)), GetHoverCardDomain());
  EXPECT_EQ(GetHoverCardsSeenCount(), 1);
  ASSERT_NE(nullptr, hover_card()->GetWidget());
  EXPECT_TRUE(hover_card()->GetWidget()->IsVisible());
}
