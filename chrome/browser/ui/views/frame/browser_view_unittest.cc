// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/version_info/channel.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/recently_audible_helper.h"
#endif

namespace {

// Tab strip bounds depend on the window frame sizes.
gfx::Point ExpectedTabStripOrigin(BrowserView* browser_view) {
  gfx::Rect tabstrip_bounds(
      browser_view->frame()->GetBoundsForTabStrip(browser_view->tabstrip()));
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToTarget(browser_view->parent(),
                                    browser_view,
                                    &tabstrip_origin);
  return tabstrip_origin;
}

// Helper function to take a printf-style format string and substitute the
// browser name (like "Chromium" or "Google Chrome") for %s, and return the
// result as a base::string16.
base::string16 SubBrowserName(const char* fmt) {
  return base::UTF8ToUTF16(base::StringPrintf(
      fmt, l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str()));
}

}  // namespace

typedef TestWithBrowserView BrowserViewTest;

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // The window is owned by the native widget, not the test class.
  EXPECT_FALSE(window());

  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  EXPECT_FALSE(browser_view()->IsIncognito());
  EXPECT_FALSE(browser_view()->IsGuestSession());
  EXPECT_TRUE(browser_view()->IsBrowserTypeNormal());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarAnimating());
}

// Test layout of the top-of-window UI.
TEST_F(BrowserViewTest, BrowserViewLayout) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  // |browser_view_| owns the Browser, not the test class.
  Browser* browser = browser_view()->browser();
  TopContainerView* top_container = browser_view()->top_container();
  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::View* contents_container =
      browser_view()->GetContentsContainerForTest();
  views::WebView* contents_web_view = browser_view()->contents_web_view();
  views::WebView* devtools_web_view =
      browser_view()->GetDevToolsWebViewForTest();

  // Start with a single tab open to a normal page.
  AddTab(browser, GURL("about:blank"));

  // Verify the view hierarchy.
  EXPECT_EQ(top_container, browser_view()->tabstrip()->parent());
  EXPECT_EQ(top_container, browser_view()->toolbar()->parent());
  EXPECT_EQ(top_container, browser_view()->GetBookmarkBarView()->parent());
  EXPECT_EQ(browser_view(), browser_view()->infobar_container()->parent());

  // Find bar host is at the front of the view hierarchy, followed by the
  // infobar container and then top container.
  EXPECT_EQ(browser_view()->child_count() - 1,
            browser_view()->GetIndexOf(browser_view()->find_bar_host_view()));
  EXPECT_EQ(browser_view()->child_count() - 2,
            browser_view()->GetIndexOf(browser_view()->infobar_container()));

  // Verify basic layout.
  EXPECT_EQ(0, top_container->x());
  EXPECT_EQ(0, top_container->y());
  EXPECT_EQ(browser_view()->width(), top_container->width());
  // Tabstrip layout varies based on window frame sizes.
  gfx::Point expected_tabstrip_origin = ExpectedTabStripOrigin(browser_view());
  EXPECT_EQ(expected_tabstrip_origin.x(), tabstrip->x());
  EXPECT_EQ(expected_tabstrip_origin.y(), tabstrip->y());
  EXPECT_EQ(0, toolbar->x());
  EXPECT_EQ(
      tabstrip->bounds().bottom() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
      toolbar->y());
  EXPECT_EQ(0, contents_container->x());
  EXPECT_EQ(toolbar->bounds().bottom(), contents_container->y());
  EXPECT_EQ(top_container->bounds().bottom(), contents_container->y());
  EXPECT_EQ(0, devtools_web_view->x());
  EXPECT_EQ(0, devtools_web_view->y());
  EXPECT_EQ(0, contents_web_view->x());
  EXPECT_EQ(0, contents_web_view->y());

  // Verify bookmark bar visibility.
  BookmarkBarView* bookmark_bar = browser_view()->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  EXPECT_EQ(devtools_web_view->y(), bookmark_bar->height());
  EXPECT_EQ(GetLayoutConstant(BOOKMARK_BAR_HEIGHT),
            bookmark_bar->GetMinimumSize().height());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());

  // Bookmark bar is reparented to BrowserView on NTP.
  NavigateAndCommitActiveTabWithTitle(browser,
                                      GURL(chrome::kChromeUINewTabURL),
                                      base::string16());
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_TRUE(bookmark_bar->IsDetached());
  EXPECT_EQ(browser_view(), bookmark_bar->parent());

  // Find bar host is still at the front of the view hierarchy, followed by the
  // infobar container and then top container.
  EXPECT_EQ(browser_view()->child_count() - 1,
            browser_view()->GetIndexOf(browser_view()->find_bar_host_view()));
  EXPECT_EQ(browser_view()->child_count() - 2,
            browser_view()->GetIndexOf(browser_view()->infobar_container()));

  // Bookmark bar layout on NTP.
  EXPECT_EQ(0, bookmark_bar->x());
  EXPECT_EQ(tabstrip->bounds().bottom() + toolbar->height() -
                GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
            bookmark_bar->y());
  EXPECT_EQ(bookmark_bar->height() + bookmark_bar->y(),
            contents_container->y());
  EXPECT_EQ(contents_web_view->y(), devtools_web_view->y());

  // Bookmark bar is parented back to top container on normal page.
  NavigateAndCommitActiveTabWithTitle(browser,
                                      GURL("about:blank"),
                                      base::string16());
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  EXPECT_EQ(top_container, bookmark_bar->parent());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !defined(OS_MACOSX)
// Test that repeated accelerators are processed or ignored depending on the
// commands that they refer to. The behavior for different commands is dictated
// by IsCommandRepeatable() in chrome/browser/ui/views/accelerator_table.h.
TEST_F(BrowserViewTest, RepeatedAccelerators) {
  // A non-repeated Ctrl-L accelerator should be processed.
  const ui::Accelerator kLocationAccel(ui::VKEY_L, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_TRUE(browser_view()->AcceleratorPressed(kLocationAccel));

  // If the accelerator is repeated, it should be ignored.
  const ui::Accelerator kLocationRepeatAccel(
      ui::VKEY_L, ui::EF_PLATFORM_ACCELERATOR | ui::EF_IS_REPEAT);
  EXPECT_FALSE(browser_view()->AcceleratorPressed(kLocationRepeatAccel));

  // A repeated Ctrl-Tab accelerator should be processed.
  const ui::Accelerator kNextTabRepeatAccel(
      ui::VKEY_TAB, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  EXPECT_TRUE(browser_view()->AcceleratorPressed(kNextTabRepeatAccel));
}
#endif  // !defined(OS_MACOSX)

// Test that bookmark bar view becomes invisible when closing the browser.
TEST_F(BrowserViewTest, BookmarkBarInvisibleOnShutdown) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  Browser* browser = browser_view()->browser();
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());

  AddTab(browser, GURL("about:blank"));
  EXPECT_EQ(1, tab_strip_model->count());

  BookmarkBarView* bookmark_bar = browser_view()->GetBookmarkBarView();
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->visible());

  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(0, tab_strip_model->count());
  EXPECT_FALSE(bookmark_bar->visible());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

TEST_F(BrowserViewTest, AccessibleWindowTitle) {
  EXPECT_EQ(SubBrowserName("Untitled - %s"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::STABLE, browser()->profile()));
  EXPECT_EQ(SubBrowserName("Untitled - %s Beta"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::BETA, browser()->profile()));
  EXPECT_EQ(SubBrowserName("Untitled - %s Dev"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::DEV, browser()->profile()));
  EXPECT_EQ(SubBrowserName("Untitled - %s Canary"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::CANARY, browser()->profile()));

  AddTab(browser(), GURL("about:blank"));
  EXPECT_EQ(SubBrowserName("about:blank - %s"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::STABLE, browser()->profile()));

  Tab* tab = browser_view()->tabstrip()->tab_at(0);
  TabRendererData start_media;
  start_media.alert_state = TabAlertState::AUDIO_PLAYING;
  tab->SetData(std::move(start_media));
  EXPECT_EQ(SubBrowserName("about:blank - Audio playing - %s"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::STABLE, browser()->profile()));

  TabRendererData network_error;
  network_error.network_state = TabNetworkState::kError;
  tab->SetData(std::move(network_error));
  EXPECT_EQ(SubBrowserName("about:blank - Network error - %s Beta"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::BETA, browser()->profile()));

  TestingProfile* profile = profile_manager()->CreateTestingProfile("Sadia");
  EXPECT_EQ(SubBrowserName("about:blank - Network error - %s Dev - Sadia"),
            browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
                version_info::Channel::DEV, profile));

  EXPECT_EQ(
      SubBrowserName("about:blank - Network error - %s Canary (Incognito)"),
      browser_view()->GetAccessibleWindowTitleForChannelAndProfile(
          version_info::Channel::CANARY,
          TestingProfile::Builder().BuildIncognito(profile)));
}

#if defined(OS_MACOSX)
// Tests that audio playing state is reflected in the "Window" menu on Mac.
TEST_F(BrowserViewTest, TitleAudioIndicators) {
  base::string16 playing_icon = base::WideToUTF16(L"\U0001F50A");
  base::string16 muted_icon = base::WideToUTF16(L"\U0001F507");

  AddTab(browser_view()->browser(), GURL("about:blank"));
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  RecentlyAudibleHelper* audible_helper =
      RecentlyAudibleHelper::FromWebContents(contents);

  audible_helper->SetNotRecentlyAudibleForTesting();
  EXPECT_EQ(browser_view()->GetWindowTitle().find(playing_icon),
            base::string16::npos);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(muted_icon),
            base::string16::npos);

  audible_helper->SetCurrentlyAudibleForTesting();
  EXPECT_NE(browser_view()->GetWindowTitle().find(playing_icon),
            base::string16::npos);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(muted_icon),
            base::string16::npos);

  audible_helper->SetRecentlyAudibleForTesting();
  contents->SetAudioMuted(true);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(playing_icon),
            base::string16::npos);
  EXPECT_NE(browser_view()->GetWindowTitle().find(muted_icon),
            base::string16::npos);
}
#endif

class BrowserViewHostedAppTest : public TestWithBrowserView {
 public:
  BrowserViewHostedAppTest() : TestWithBrowserView(Browser::TYPE_POPUP, true) {}
  ~BrowserViewHostedAppTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserViewHostedAppTest);
};

// Test basic layout for hosted apps.
TEST_F(BrowserViewHostedAppTest, Layout) {
  // Add a tab because the browser starts out without any tabs at all.
  AddTab(browser(), GURL("about:blank"));

  views::View* contents_container =
      browser_view()->GetContentsContainerForTest();

  // The tabstrip, toolbar and bookmark bar should not be visible for hosted
  // apps.
  EXPECT_FALSE(browser_view()->tabstrip()->visible());
  EXPECT_FALSE(browser_view()->toolbar()->visible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());

  gfx::Point header_offset;
  views::View::ConvertPointToTarget(
      browser_view(),
      browser_view()->frame()->non_client_view()->frame_view(),
      &header_offset);

  // The position of the bottom of the header (the bar with the window
  // controls) in the coordinates of BrowserView.
  int bottom_of_header =
      browser_view()->frame()->GetTopInset() - header_offset.y();

  // The web contents should be flush with the bottom of the header.
  EXPECT_EQ(bottom_of_header, contents_container->y());

  // The find bar should butt against the 1px header/web-contents separator at
  // the bottom of the header.
  EXPECT_EQ(browser_view()->GetFindBarBoundingBox().y(),
            browser_view()->frame()->GetTopInset());
}
