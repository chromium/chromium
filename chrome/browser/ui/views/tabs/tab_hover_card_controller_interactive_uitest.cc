// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/views/tabs/fade_footer_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_test_util.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"

namespace {
constexpr char16_t kTabTitle[] = u"Test Tab 2";
constexpr char16_t kTabDomain[] = u"example.com";
constexpr char kTabUrl[] = "http://example.com/path/to/document.html";

TabRendererData MakeTabRendererData() {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = kTabTitle;
  new_tab_data.last_committed_url = GURL(kTabUrl);
  new_tab_data.alert_state = {TabAlertState::AUDIO_PLAYING};
  return new_tab_data;
}
}  // namespace

class TabHoverCardInteractiveUiTest : public InteractiveBrowserTest,
                                      public test::TabHoverCardTestUtil {
 public:
  ~TabHoverCardInteractiveUiTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    scoped_feature_list_.InitWithFeatures(
        {performance_manager::features::kDiscardedTabTreatment,
         performance_manager::features::kMemoryUsageInHovercards},
        {});
    InteractiveBrowserTest::SetUp();
  }

  // Start the test by moving the mouse to a location where it will not be
  // hovering the tabstrip. All subsequent interactions will be simulated.
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    Tab::SetShowHoverCardOnMouseHoverForTesting(true);
    embedded_test_server()->StartAcceptingConnections();
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    gfx::Point upper_left;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Because Ozone makes it impossible to target a point not in a window in
    // tests, instead target the extreme upper left of the browser window.
    upper_left = browser()->window()->GetBounds().origin();
#endif
    ui_controls::SendMouseMoveNotifyWhenDone(upper_left.x(), upper_left.y(),
                                             run_loop.QuitClosure());
    run_loop.Run();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  MultiStep HoverTabAt(int index) {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/1396074): Fix for mac
    return Steps(Do(base::BindLambdaForTesting(
        [=]() { SimulateHoverTab(browser(), index); })));
#else
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
#endif
  }

  MultiStep UnhoverTab() {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/1396074): Fix for mac
    return Steps(Do(base::BindLambdaForTesting([=]() {
      TabStrip* const tab_strip = GetTabStrip(browser());
      HoverCardDestroyedWaiter waiter(tab_strip);
      ui::MouseEvent stop_hover_event(ui::ET_MOUSE_EXITED, gfx::Point(),
                                      gfx::Point(), base::TimeTicks(),
                                      ui::EF_NONE, 0);
      static_cast<views::View*>(tab_strip)->OnMouseExited(stop_hover_event);
      waiter.Wait();
    })));
#else
    return Steps(MoveMouseTo(kNewTabButtonElementId));
#endif
  }

  StepBuilder CheckHovercardIsOpen() {
    return WaitForShow(TabHoverCardBubbleView::kHoverCardBubbleElementId);
  }

  StepBuilder CheckHovercardIsClosed() {
    return WaitForHide(TabHoverCardBubbleView::kHoverCardBubbleElementId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(USE_AURA)

// Verify that the hover card is not visible when any key is pressed.
// Because this test depends on Aura event handling, it is not performed on Mac.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardHidesOnAnyKeyPressInSameWindow) {
  RunTestSequence(HoverTabAt(0), CheckHovercardIsOpen(),
                  Check(base::BindLambdaForTesting([=]() {
                    return ui_test_utils::SendKeyPressSync(
                        browser(), ui::VKEY_DOWN, false, false, false, false);
                  })),
                  CheckHovercardIsClosed());
}

#endif

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardHidesOnMouseExit) {
  RunTestSequence(HoverTabAt(0), CheckHovercardIsOpen(), UnhoverTab(),
                  CheckHovercardIsClosed());
}

// TODO(crbug.com/1050765): test may be flaky on Linux and/or ChromeOS.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardShownOnTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);
}

// TODO(crbug.com/1050765): test may be flaky on Linux and/or ChromeOS.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardVisibleOnTabCloseButtonFocusAfterTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);
  tab_strip->GetFocusManager()->SetFocusedView(tab->close_button_);
  EXPECT_TRUE(IsHoverCardVisible(tab_strip));
}

// Verify hover card is visible when tab is focused and a key is pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       WidgetVisibleOnKeyPressAfterTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
  tab->OnKeyPressed(key_event);
  EXPECT_TRUE(IsHoverCardVisible(tab_strip));
}

// Verify hover card is not visible when tab is focused and the mouse is
// pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       WidgetNotVisibleOnMousePressAfterTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, 0);
  tab->OnMousePressed(click_event);
  EXPECT_FALSE(IsHoverCardVisible(tab_strip));
}

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       WidgetNotVisibleOnMousePressAfterHover) {
  RunTestSequence(HoverTabAt(0), CheckHovercardIsOpen(),
                  SelectTab(kTabStripElementId, 0), CheckHovercardIsClosed());
}

// TODO(crbug.com/1050765): test may be flaky on Linux and/or ChromeOS.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardVisibleOnTabFocusFromKeyboardAccelerator) {
  TabStrip* const tab_strip = GetTabStrip(browser());

  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  // Cycle focus until it reaches a tab.
  while (!tab_strip->IsFocusInTabs())
    browser()->command_controller()->ExecuteCommand(IDC_FOCUS_NEXT_PANE);

  WaitForHoverCardVisible(tab_strip);

  // Move focus forward to the close button or next tab dependent on window
  // size.
  tab_strip->AcceleratorPressed(ui::Accelerator(ui::VKEY_RIGHT, ui::EF_NONE));
  EXPECT_TRUE(IsHoverCardVisible(tab_strip));
}

// TODO(crbug.com/1050765): test may be flaky on Windows.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       InactiveWindowStaysInactiveOnHover) {
  const BrowserList* active_browser_list = BrowserList::GetInstance();

  // Open a second browser window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list->size());

  // Choose one browser to be active; the other to be inactive.
  Browser* active_window = active_browser_list->get(0);
  Browser* inactive_window = active_browser_list->get(1);

  // Activate the active browser and wait for the inactive browser to be
  // inactive.
  views::test::WidgetActivationWaiter waiter(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->frame(), false);
  BrowserView::GetBrowserViewForBrowser(active_window)->Activate();
  waiter.Wait();
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());

  // Simulate hovering the inactive tabstrip and wait for the hover card to
  // appear. The inactive browser should remain inactive.
  SimulateHoverTab(inactive_window, 0);
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
}

// TODO(crbug.com/1050765): test may be flaky on Windows.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       UpdatesHoverCardOnHoverDifferentTab) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  tab_strip->SetTabData(1, MakeTabRendererData());

  SimulateHoverTab(browser(), 0);

  auto* const hover_card = SimulateHoverTab(browser(), 1);
  EXPECT_EQ(kTabTitle, hover_card->GetTitleTextForTesting());
  EXPECT_EQ(kTabDomain, hover_card->GetDomainTextForTesting());
  EXPECT_EQ(tab_strip->tab_at(1), hover_card->GetAnchorView());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest, HoverCardFooterUpdates) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  tab_strip->SetTabData(1, MakeTabRendererData());

  auto* const hover_card = SimulateHoverTab(browser(), 1);
  FadeAlertFooterRow* alert_row =
      hover_card->footer_view_->GetAlertRow()->primary_view_;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING),
      alert_row->footer_label_->GetText());
  EXPECT_FALSE(alert_row->icon_->GetImageModel().IsEmpty());

  // Hover card footer should update when we hover over another tab that is
  // not playing audio
  SimulateHoverTab(browser(), 0);
  EXPECT_TRUE(alert_row->footer_label_->GetText().empty());
  EXPECT_TRUE(alert_row->icon_->GetImageModel().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardFooterShowsDiscardStatus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabRendererData tab_renderer_data = MakeTabRendererData();
  tab_renderer_data.should_show_discard_status = true;
  tab_strip->SetTabData(1, tab_renderer_data);

  auto* const hover_card = SimulateHoverTab(browser(), 1);
  FadePerformanceFooterRow* performance_row =
      hover_card->footer_view_->GetPerformanceRow()->primary_view_;
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB),
            performance_row->footer_label_->GetText());
  EXPECT_FALSE(performance_row->icon_->GetImageModel().IsEmpty());

  // Hover card footer should update when we hover over another tab that is
  // not discarded
  SimulateHoverTab(browser(), 0);
  EXPECT_TRUE(performance_row->footer_label_->GetText().empty());
  EXPECT_TRUE(performance_row->icon_->GetImageModel().IsEmpty());

  // Show discard status with memory savings
  tab_renderer_data.discarded_memory_savings_in_bytes = 1000;
  tab_strip->SetTabData(1, tab_renderer_data);
  SimulateHoverTab(browser(), 1);
  EXPECT_EQ(
      l10n_util::FormatString(
          l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB_MEMORY_SAVINGS),
          {ui::FormatBytes(
              tab_renderer_data.discarded_memory_savings_in_bytes)},
          nullptr),
      performance_row->footer_label_->GetText());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardFooterShowsMemoryUsage) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  uint64_t bytes_used = 1000;
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  auto* const resource_usage_tab_helper =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          ResourceUsageTabHelper::FromWebContents(web_contents);
  resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);

  // Show memory usage without savings
  auto* const hover_card = SimulateHoverTab(browser(), 1);
  FadePerformanceFooterRow* performance_row =
      hover_card->footer_view_->GetPerformanceRow()->primary_view_;
  EXPECT_EQ(l10n_util::FormatString(
                l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_MEMORY_USAGE),
                {ui::FormatBytes(bytes_used)}, nullptr),
            performance_row->footer_label_->GetText());
  EXPECT_FALSE(performance_row->icon_->GetImageModel().IsEmpty());

  // Hover card updates and shows high memory usage when card is still open
  bytes_used =
      performance_manager::features::kMemoryUsageInHovercardsHighThresholdBytes
          .Get() +
      100;
  resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);
  GetTabStrip(browser())
      ->hover_card_controller_for_testing()
      ->OnMemoryMetricsRefreshed();
  EXPECT_EQ(l10n_util::FormatString(
                l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_HIGH_MEMORY_USAGE),
                {ui::FormatBytes(bytes_used)}, nullptr),
            performance_row->footer_label_->GetText());
}

using TabHoverCardBubbleViewMetricsTest = TabHoverCardInteractiveUiTest;

IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewMetricsTest,
                       HoverCardsSeenRatioMetric) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  SimulateHoverTab(browser(), 0);

  EXPECT_EQ(1, GetHoverCardsSeenCount(browser()));

  SimulateHoverTab(browser(), 1);

  EXPECT_EQ(2, GetHoverCardsSeenCount(browser()));

  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(1);
  tab_strip->SetSelection(selection);

  TabHoverCardBubbleView* const hover_card = GetHoverCard(tab_strip);
  EXPECT_FALSE(hover_card && hover_card->GetWidget()->IsVisible());
  EXPECT_EQ(0, GetHoverCardsSeenCount(browser()));
}

// Tests for tabs showing interstitials to check whether the URL in the hover
// card is displayed or hidden as appropriate.
class TabHoverCardBubbleViewInterstitialBrowserTest
    : public TabHoverCardInteractiveUiTest {
 public:
  ~TabHoverCardBubbleViewInterstitialBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_mismatched_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_mismatched_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_->AddDefaultHandlers(GetChromeTestDataDir());

    TabHoverCardInteractiveUiTest::SetUpOnMainThread();
    lookalikes::InitializeSafetyTipConfig();
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
  //  Navigate the tab to a lookalike URL and check the hover card. The domain
  //  field must be empty.
  static constexpr char kLookalikeDomain[] = "googlé.com";
  static constexpr char kUrlPath[] = "/empty.html";
  const GURL url = embedded_test_server()->GetURL(kLookalikeDomain, kUrlPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Open another tab.
  chrome::NewTab(browser());
  auto* const hover_card = SimulateHoverTab(browser(), 0);

  EXPECT_TRUE(hover_card->GetDomainTextForTesting().empty());
  EXPECT_EQ(1, GetHoverCardsSeenCount(browser()));
}

// Verify that the domain field of tab's hover card is not empty on other types
// of interstitials (here, SSL).
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInterstitialBrowserTest,
                       SSLInterstitial_ShouldShowHoverCardUrl) {
  ASSERT_TRUE(https_server_mismatched()->Start());
  // Navigate the tab to an SSL error.
  static constexpr char kBadSSLDomain[] = "site.test";
  static constexpr char kUrlPath[] = "/empty.html";
  const GURL url = https_server_mismatched()->GetURL(kBadSSLDomain, kUrlPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(tab));

  // Open another tab.
  chrome::NewTab(browser());
  auto* const hover_card = SimulateHoverTab(browser(), 0);

  EXPECT_EQ(base::UTF8ToUTF16(net::GetHostAndPort(url)),
            hover_card->GetDomainTextForTesting());
  EXPECT_EQ(1, GetHoverCardsSeenCount(browser()));
}
