// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_interactive_test_mixin.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fade_footer_view.h"
#include "chrome/browser/ui/views/tabs/fade_label_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_test_util.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
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
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#endif

namespace {
constexpr char16_t kTabTitle[] = u"Test Tab 2";
constexpr char16_t kTabDomain[] = u"example.com";
constexpr char kTabUrl[] = "http://example.com/path/to/document.html";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

TabRendererData MakeTabRendererData() {
  TabRendererData new_tab_data = TabRendererData();
  new_tab_data.title = kTabTitle;
  new_tab_data.last_committed_url = GURL(kTabUrl);
  new_tab_data.alert_state = {TabAlertState::AUDIO_PLAYING};
  return new_tab_data;
}

}  // namespace

class TabHoverCardInteractiveUiTest
    : public MemorySaverInteractiveTestMixin<InteractiveBrowserTest>,
      public test::TabHoverCardTestUtil {
 public:
  ~TabHoverCardInteractiveUiTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
    MemorySaverInteractiveTestMixin::SetUp();
  }

  // Start the test by moving the mouse to a location where it will not be
  // hovering the tabstrip. All subsequent interactions will be simulated.
  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    Tab::SetShowHoverCardOnMouseHoverForTesting(true);
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
    MemorySaverInteractiveTestMixin::TearDownOnMainThread();
  }

  MultiStep FinishTabstripAnimations() {
    return Steps(WaitForShow(kTabStripElementId),
                 WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                   tab_strip->StopAnimating(true);
                 }));
  }

  auto HoverTabAt(int index) {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/358199067): Fix for mac
    return Steps(Do(base::BindLambdaForTesting(
        [=, this]() { SimulateHoverTab(browser(), index); })));
#else
    const char kTabToHover[] = "Tab to hover";
    return Steps(
        FinishTabstripAnimations(),
        NameDescendantViewByType<Tab>(kTabStripElementId, kTabToHover, index),
        MoveMouseTo(kTabToHover));
#endif
  }

  auto UnhoverTab() {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/358199067): Fix for mac
    return Steps(Do(base::BindLambdaForTesting([=, this]() {
      TabStrip* const tab_strip = GetTabStrip(browser());
      HoverCardDestroyedWaiter waiter(tab_strip);
      ui::MouseEvent stop_hover_event(ui::EventType::kMouseExited, gfx::Point(),
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
  RunTestSequence(InstrumentTab(kFirstTabContents, 0),
                  NavigateWebContents(kFirstTabContents,
                                      GURL(chrome::kChromeUINewTabURL)),
                  HoverTabAt(0), CheckHovercardIsOpen(),
                  Check(base::BindLambdaForTesting([=, this]() {
                    return ui_test_utils::SendKeyPressSync(
                        browser(), ui::VKEY_DOWN, false, false, false, false);
                  })),
                  CheckHovercardIsClosed());
}

#endif

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardHidesOnMouseExit) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUINewTabURL)),
      HoverTabAt(0), CheckHovercardIsOpen(), UnhoverTab(),
      CheckHovercardIsClosed());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HoverCardShownOnTabFocus DISABLED_HoverCardShownOnTabFocus
#else
#define MAYBE_HoverCardShownOnTabFocus HoverCardShownOnTabFocus
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       MAYBE_HoverCardShownOnTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);
}

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

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_SPACE, 0);
  tab->OnKeyPressed(key_event);
  EXPECT_TRUE(IsHoverCardVisible(tab_strip));
}

// Verify hover card thumbnail is not visible on active tabs.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       ThumbnailNotVisibileOnActiveTabs) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  EXPECT_TRUE(tab->IsActive());
  WaitForHoverCardVisible(tab_strip);
  views::View* const thumbnail_view_ =
      GetHoverCard(tab_strip)->GetThumbnailViewForTesting();
  CHECK(thumbnail_view_);
  EXPECT_FALSE(thumbnail_view_->GetVisible());
}

// Verify hover card is not visible when tab is focused and the mouse is
// pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       WidgetNotVisibleOnMousePressAfterTabFocus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  Tab* const tab = tab_strip->tab_at(0);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks(), ui::EF_NONE, 0);
  tab->OnMousePressed(click_event);
  EXPECT_FALSE(IsHoverCardVisible(tab_strip));
}

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       WidgetNotVisibleOnMousePressAfterHover) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GURL(chrome::kChromeUINewTabURL)),
      HoverTabAt(0), CheckHovercardIsOpen(), SelectTab(kTabStripElementId, 0),
      CheckHovercardIsClosed());
}

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

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       InactiveWindowStaysInactiveOnHover) {
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetFocusedTabStripModelForTesting(nullptr);
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
  BrowserView::GetBrowserViewForBrowser(active_window)->Activate();
  views::test::WaitForWidgetActive(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->frame(), false);
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());

  // Simulate hovering the inactive tabstrip and wait for the hover card to
  // appear. The inactive browser should remain inactive.
  SimulateHoverTab(inactive_window, 0);
  ASSERT_FALSE(
      BrowserView::GetBrowserViewForBrowser(inactive_window)->IsActive());
}

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

IN_PROC_BROWSER_TEST_F(TabHoverCardInteractiveUiTest,
                       HoverCardDoesNotHaveFooterView) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabRendererData tab_data = TabRendererData();
  tab_data.title = kTabTitle;
  tab_data.last_committed_url = GURL(kTabUrl);
  tab_strip->SetTabData(1, tab_data);

  auto* const hover_card = SimulateHoverTab(browser(), 1);
  EXPECT_FALSE(hover_card->GetFooterViewForTesting()->GetVisible());
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

class TabHoverCardFadeFooterInteractiveUiTest
    : public TabHoverCardInteractiveUiTest {
 public:
  FadeAlertFooterRow* GetPrimaryAlertRowFromHoverCard(
      TabHoverCardBubbleView* bubble) {
    return bubble->GetFooterViewForTesting()
        ->GetAlertRowForTesting()
        ->GetPrimaryViewForTesting();
  }

  FadePerformanceFooterRow* GetPrimaryPerformanceRowFromHoverCard(
      TabHoverCardBubbleView* bubble) {
    return bubble->GetFooterViewForTesting()
        ->GetPerformanceRowForTesting()
        ->GetPrimaryViewForTesting();
  }

  auto CheckAlertRowLabel(std::u16string expected_text, bool has_position) {
    return Steps(
        WaitForShow(TabHoverCardBubbleView::kHoverCardBubbleElementId),
        CheckView(
            TabHoverCardBubbleView::kHoverCardBubbleElementId,
            [=, this](TabHoverCardBubbleView* bubble) {
              views::Label* const alert_label =
                  GetPrimaryAlertRowFromHoverCard(bubble)->footer_label();
              return alert_label->GetText().find(expected_text) !=
                     std::string::npos;
            },
            has_position));
  }

  auto ForceTabDataRefreshMemoryMetrics() {
    return Do([]() {
      TabResourceUsageRefreshWaiter waiter;
      waiter.Wait();
    });
  }
};

// Mocks the hover card footer is playing audio and verifies that
// the correct string is shown for the corresponding tab alert.
IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       HoverCardFooterUpdatesTabAlertStatus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  tab_strip->SetTabData(1, MakeTabRendererData());

  FadeAlertFooterRow* const alert_row =
      GetPrimaryAlertRowFromHoverCard(SimulateHoverTab(browser(), 1));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING),
      alert_row->footer_label()->GetText());
  EXPECT_FALSE(alert_row->icon()->GetImageModel().IsEmpty());

  // Hover card footer should update when we hover over another tab that is
  // not playing audio
  SimulateHoverTab(browser(), 0);
  EXPECT_TRUE(alert_row->footer_label()->GetText().empty());
  EXPECT_TRUE(alert_row->icon()->GetImageModel().IsEmpty());
}

class TabHoverCardFadeFooterWithDiscardInteractiveUiTest
    : public TabHoverCardFadeFooterInteractiveUiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    TabHoverCardFadeFooterInteractiveUiTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Mocks that a tab is discarded and verifies that the correct string
// for a discarded tab and discarded tab with memory usage is displayed
// on the hover card
IN_PROC_BROWSER_TEST_P(TabHoverCardFadeFooterWithDiscardInteractiveUiTest,
                       HoverCardFooterShowsDiscardStatus) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabRendererData tab_renderer_data = MakeTabRendererData();
  tab_renderer_data.should_show_discard_status = true;
  tab_strip->SetTabData(1, tab_renderer_data);

  FadeAlertFooterRow* const alert_row =
      GetPrimaryAlertRowFromHoverCard(SimulateHoverTab(browser(), 1));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB),
            alert_row->footer_label()->GetText());
  EXPECT_FALSE(alert_row->icon()->GetImageModel().IsEmpty());

  // Clear the memory usage data from tab 0 if it was set, otherwise the
  // performance row won't be empty.
  TabRendererData tab_0_data = tab_strip->tab_at(0)->data();
  tab_0_data.tab_resource_usage = nullptr;
  tab_strip->SetTabData(0, tab_0_data);

  // Hover card footer should update when we hover over another tab that is
  // not discarded
  SimulateHoverTab(browser(), 0);
  EXPECT_TRUE(alert_row->footer_label()->GetText().empty());
  EXPECT_TRUE(alert_row->icon()->GetImageModel().IsEmpty());

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
      alert_row->footer_label()->GetText());
}

// With memory usage in hovercards pref set to enabled, mocks a tab with normal
// memory usage and verifies that the string for normal memory usage is shown on
// the hover card. Also tests that the hover card updates this string and use
// the high memory usage string instead when the tab uses memory above the
// threshold.
IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       HoverCardFooterMemoryUsagePrefEnabled) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kHoverCardMemoryUsageEnabled, true);
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  uint64_t bytes_used = 1000;
  auto* const tab_resource_usage_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(1));
  tab_resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);

  // Show memory usage without savings
  FadePerformanceFooterRow* const performance_row =
      GetPrimaryPerformanceRowFromHoverCard(SimulateHoverTab(browser(), 1));
  EXPECT_EQ(l10n_util::FormatString(
                l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_MEMORY_USAGE),
                {ui::FormatBytes(bytes_used)}, nullptr),
            performance_row->footer_label()->GetText());
  EXPECT_FALSE(performance_row->icon()->GetImageModel().IsEmpty());

  // Hover card updates and shows high memory usage when card is still open
  bytes_used = TabResourceUsage::kHighMemoryUsageThresholdBytes + 100;
  tab_resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);
  GetTabStrip(browser())
      ->hover_card_controller_for_testing()
      ->OnTabResourceMetricsRefreshed();
  EXPECT_EQ(l10n_util::FormatString(
                l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_HIGH_MEMORY_USAGE),
                {ui::FormatBytes(bytes_used)}, nullptr),
            performance_row->footer_label()->GetText());
}

// With memory usage in hovercards pref set to disabled, mocks a tab with normal
// memory usage and verifies that the string for normal memory usage is not
// shown on the hover card. Also tests that the hover card does show the high
// memory usage string when the tab uses memory above the threshold.
IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       HoverCardFooterMemoryUsagePrefDisabled) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kHoverCardMemoryUsageEnabled, false);
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  uint64_t bytes_used = 1000;
  auto* const tab_resource_usage_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(1));
  tab_resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);

  // Don't show memory usage
  FadePerformanceFooterRow* const performance_row =
      GetPrimaryPerformanceRowFromHoverCard(SimulateHoverTab(browser(), 1));
  EXPECT_TRUE(performance_row->footer_label()->GetText().empty());
  EXPECT_TRUE(performance_row->icon()->GetImageModel().IsEmpty());

  // Hover card updates and shows high memory usage when card is still open
  bytes_used = TabResourceUsage::kHighMemoryUsageThresholdBytes + 100;
  tab_resource_usage_tab_helper->SetMemoryUsageInBytes(bytes_used);
  GetTabStrip(browser())
      ->hover_card_controller_for_testing()
      ->OnTabResourceMetricsRefreshed();
  EXPECT_EQ(l10n_util::FormatString(
                l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_HIGH_MEMORY_USAGE),
                {ui::FormatBytes(bytes_used)}, nullptr),
            performance_row->footer_label()->GetText());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       ActiveMemoryUsageHidesOnDiscard) {
  const uint64_t bytes_used = 1;
  TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(0))
      ->SetMemoryUsageInBytes(bytes_used);

  RunTestSequence(InstrumentTab(kFirstTabContents, 0),
                  NavigateWebContents(kFirstTabContents, GetURL("a.com")),
                  AddInstrumentedTab(kSecondTabContents, GetURL("b.com")),
                  ForceTabDataRefreshMemoryMetrics(), TryDiscardTab(0),
                  HoverTabAt(0),
                  WaitForShow(FooterView::kHoverCardFooterElementId),
                  CheckAlertRowLabel(u"Inactive tab", true),
                  CheckView(
                      TabHoverCardBubbleView::kHoverCardBubbleElementId,
                      [=, this](TabHoverCardBubbleView* bubble) {
                        return GetPrimaryPerformanceRowFromHoverCard(bubble)
                            ->footer_label()
                            ->GetText()
                            .empty();
                      },
                      true));
}

// The discarded status in the hover card footer should disappear after a
// discarded tab is reloaded
IN_PROC_BROWSER_TEST_P(TabHoverCardFadeFooterWithDiscardInteractiveUiTest,
                       DiscardStatusHidesOnReload) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("a.com")),
      AddInstrumentedTab(kSecondTabContents, GetURL("b.com")), UnhoverTab(),
      ForceTabDataRefreshMemoryMetrics(), HoverTabAt(0), CheckHovercardIsOpen(),
      CheckAlertRowLabel(u"Inactive tab", false), UnhoverTab(),
      CheckHovercardIsClosed(),
      // Check that the discarded tab should update its contents to show discard
      // status
      TryDiscardTab(0), HoverTabAt(0),
      WaitForShow(FooterView::kHoverCardFooterElementId),
      CheckAlertRowLabel(u"Inactive tab", true), UnhoverTab(),
      CheckHovercardIsClosed(),
      // Ensure that hover card shows footer only for the discarded tab
      HoverTabAt(1), CheckHovercardIsOpen(),
      CheckAlertRowLabel(u"Inactive tab", false),
      // Select discarded tab
      UnhoverTab(), CheckHovercardIsClosed(), SelectTab(kTabStripElementId, 0),
      WaitForWebContentsReady(kFirstTabContents), HoverTabAt(0),
      CheckHovercardIsOpen(), CheckAlertRowLabel(u"Inactive tab", false));
}

// The hover card should stop showing memory usage data after navigating to
// another site since the data is now out of date
IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       MemoryUpdatesOnNavigation) {
  const uint64_t bytes_used = 1;
  TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(0))
      ->SetMemoryUsageInBytes(bytes_used);

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0), UnhoverTab(), HoverTabAt(0),
      CheckHovercardIsOpen(),
      WaitForShow(FooterView::kHoverCardFooterElementId), UnhoverTab(),
      CheckHovercardIsClosed(),
      NavigateWebContents(kFirstTabContents, GetURL("a.com")), HoverTabAt(0),
      CheckHovercardIsOpen(),
      CheckView(
          TabHoverCardBubbleView::kHoverCardBubbleElementId,
          [=, this](TabHoverCardBubbleView* bubble) {
            views::Label* const performance_label =
                GetPrimaryPerformanceRowFromHoverCard(bubble)->footer_label();
            return performance_label->GetText().find(l10n_util::GetStringFUTF16(
                       IDS_HOVERCARD_TAB_MEMORY_USAGE,
                       ui::FormatBytes(bytes_used))) != std::string::npos;
          },
          false));
}

// The hover card should display tab memory usage in the footer. When the user
// hovers over a tab without memory data available, the footer should update
// accordingly and stop showing memory usage
IN_PROC_BROWSER_TEST_F(TabHoverCardFadeFooterInteractiveUiTest,
                       FooterHidesOnTabWithoutMemoryUsage) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(0))
      ->SetMemoryUsageInBytes(1000);

  // Footer should show when hovering over tab with memory usage
  views::View* const footer_view =
      SimulateHoverTab(browser(), 0)->GetFooterViewForTesting();
  EXPECT_TRUE(footer_view->GetVisible());

  // Hover over a tab without memory usage
  TabResourceUsageTabHelper::FromWebContents(GetWebContentsAt(1))
      ->SetMemoryUsageInBytes(0);
  SimulateHoverTab(browser(), 1);

  // Footer should no longer be visible because there is no memory data
  EXPECT_FALSE(footer_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(
    TabHoverCardFadeFooterInteractiveUiTest,
    BackgroundTabHoverCardContentsHaveCorrectDimensions) {
  TabStrip* const tab_strip = GetTabStrip(browser());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);
  Tab* const tab = tab_strip->tab_at(1);
  TabRendererData data = tab->data();
  data.alert_state = {TabAlertState::AUDIO_PLAYING};
  tab->SetData(data);
  tab_strip->GetFocusManager()->SetFocusedView(tab);
  WaitForHoverCardVisible(tab_strip);

  auto* const hover_card =
      tab_strip->hover_card_controller_for_testing()->hover_card_for_testing();
  gfx::Size hover_card_size = hover_card->size();

  int total_children_height = 0;

  // Verify that all children of the hovercard can fit within the hovercard
  for (views::View* child : hover_card->children()) {
    EXPECT_TRUE(child->GetVisible());
    gfx::Size child_size = child->size();
    EXPECT_GT(child_size.width(), 0);
    EXPECT_LE(child_size.width(), hover_card_size.width());
    EXPECT_GT(child_size.height(), 0);
    EXPECT_LE(child_size.height(), hover_card_size.height());
    total_children_height += child_size.height();
  }

  // Verify that stacking the children within the hovercard takes up the entire
  // hover card space
  total_children_height +=
      hover_card->title_label_->GetProperty(views::kMarginsKey)->height() +
      hover_card->domain_label_->GetProperty(views::kMarginsKey)->height();
  EXPECT_EQ(hover_card_size.height(), total_children_height);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TabHoverCardSystemWebAppTest : public InteractiveBrowserTest {
 public:
  TabHoverCardSystemWebAppTest()
      : test_system_web_app_installation_(
            ash::TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp()) {}

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    Tab::SetShowHoverCardOnMouseHoverForTesting(true);
  }

 protected:
  std::unique_ptr<ash::TestSystemWebAppInstallation>
      test_system_web_app_installation_;
};

IN_PROC_BROWSER_TEST_F(TabHoverCardSystemWebAppTest,
                       HideDomainNameFromHoverCard) {
  test_system_web_app_installation_->WaitForAppInstall();
  const auto* const app_browser = web_app::LaunchWebAppBrowser(
      browser()->profile(), test_system_web_app_installation_->GetAppId());
  const char kTabToHover[] = "Tab to hover";

  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(),
      WithView(kTabStripElementId,
               [](TabStrip* tab_strip) { tab_strip->StopAnimating(true); }),
      NameDescendantViewByType<Tab>(kBrowserViewElementId, kTabToHover, 0),
      MoveMouseTo(kTabToHover),
      WaitForShow(TabHoverCardBubbleView::kHoverCardBubbleElementId),
      EnsureNotPresent(TabHoverCardBubbleView::kHoverCardDomainLabelElementId),
      MoveMouseTo(kNewTabButtonElementId),
      WaitForHide(TabHoverCardBubbleView::kHoverCardBubbleElementId));
}
#endif

INSTANTIATE_TEST_SUITE_P(
    ,
    TabHoverCardFadeFooterWithDiscardInteractiveUiTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        TabHoverCardFadeFooterWithDiscardInteractiveUiTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });
