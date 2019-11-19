// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/macros.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/test_service_manager_context.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/controls/webview/webview.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/recently_audible_helper.h"
#endif

namespace {

// Tab strip bounds depend on the window frame sizes.
gfx::Point ExpectedTabStripRegionOrigin(BrowserView* browser_view) {
  gfx::Rect tabstrip_bounds(browser_view->frame()->GetBoundsForTabStripRegion(
      browser_view->tabstrip()));
  gfx::Point tabstrip_region_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToTarget(browser_view->parent(), browser_view,
                                    &tabstrip_region_origin);
  return tabstrip_region_origin;
}

// Helper function to take a printf-style format string and substitute the
// browser name (like "Chromium" or "Google Chrome") for %s, and return the
// result as a base::string16.
base::string16 SubBrowserName(const char* fmt) {
  return base::UTF8ToUTF16(base::StringPrintf(
      fmt, l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str()));
}

}  // namespace

class BrowserViewTest : public TestWithBrowserView {
 public:
  BrowserViewTest() = default;
  ~BrowserViewTest() override = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();
    test_service_manager_context_ =
        std::make_unique<content::TestServiceManagerContext>();
  }

  void TearDown() override {
    // Must be reset before browser thread teardown.
    test_service_manager_context_.reset();
    TestWithBrowserView::TearDown();
  }

 private:
  // WebContentsImpl accesses
  // content::ServiceManagerConnection::GetForProcess(), so we must make sure it
  // is instantiated.
  std::unique_ptr<content::TestServiceManagerContext>
      test_service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewTest);
};

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
TEST_F(BrowserViewTest, DISABLED_BrowserViewLayout) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  // |browser_view_| owns the Browser, not the test class.
  Browser* browser = browser_view()->browser();
  TopContainerView* top_container = browser_view()->top_container();
  TabStrip* tabstrip = browser_view()->tabstrip();
  views::View* tabstrip_region = browser_view()->tabstrip()->parent();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::View* contents_container =
      browser_view()->GetContentsContainerForTest();
  views::WebView* contents_web_view = browser_view()->contents_web_view();
  views::WebView* devtools_web_view =
      browser_view()->GetDevToolsWebViewForTest();

  // Start with a single tab open to a normal page.
  AddTab(browser, GURL("about:blank"));

  // Verify the view hierarchy.
  EXPECT_EQ(top_container, tabstrip_region->parent());
  EXPECT_EQ(tabstrip_region, tabstrip->parent());
  EXPECT_EQ(top_container, browser_view()->toolbar()->parent());
  EXPECT_EQ(top_container, browser_view()->GetBookmarkBarView()->parent());
  EXPECT_EQ(browser_view(), browser_view()->infobar_container()->parent());

  // Find bar host is at the front of the view hierarchy, followed by the
  // infobar container and then top container.
  ASSERT_GE(browser_view()->children().size(), 2U);
  auto child = browser_view()->children().crbegin();
  EXPECT_EQ(browser_view()->find_bar_host_view(), *child++);
  EXPECT_EQ(browser_view()->infobar_container(), *child);

  // Verify basic layout.
  EXPECT_EQ(0, top_container->x());
  EXPECT_EQ(0, top_container->y());
  EXPECT_EQ(browser_view()->width(), top_container->width());
  // Tabstrip layout varies based on window frame sizes.
  gfx::Point expected_tabstrip_region_origin =
      ExpectedTabStripRegionOrigin(browser_view());
  EXPECT_EQ(expected_tabstrip_region_origin.x(), tabstrip_region->x());
  EXPECT_EQ(expected_tabstrip_region_origin.y(), tabstrip_region->y());
  EXPECT_EQ(0, toolbar->x());
  EXPECT_EQ(tabstrip_region->bounds().bottom() -
                GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
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
  EXPECT_FALSE(bookmark_bar->GetVisible());
  EXPECT_EQ(devtools_web_view->y(), bookmark_bar->height());
  EXPECT_EQ(GetLayoutConstant(BOOKMARK_BAR_HEIGHT),
            bookmark_bar->GetMinimumSize().height());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->GetVisible());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->GetVisible());

  // The NTP should be treated the same as any other page.
  NavigateAndCommitActiveTabWithTitle(browser,
                                      GURL(chrome::kChromeUINewTabURL),
                                      base::string16());
  EXPECT_FALSE(bookmark_bar->GetVisible());
  EXPECT_EQ(top_container, bookmark_bar->parent());

  // Find bar host is still at the front of the view hierarchy, followed by the
  // infobar container and then top container.
  ASSERT_GE(browser_view()->children().size(), 2U);
  child = browser_view()->children().crbegin();
  EXPECT_EQ(browser_view()->find_bar_host_view(), *child++);
  EXPECT_EQ(browser_view()->infobar_container(), *child);

  // Bookmark bar layout on NTP.
  EXPECT_EQ(0, bookmark_bar->x());
  EXPECT_EQ(tabstrip_region->bounds().bottom() + toolbar->height() -
                GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
            bookmark_bar->y());
  EXPECT_EQ(bookmark_bar->height() + bookmark_bar->y(),
            contents_container->y());
  EXPECT_EQ(contents_web_view->y(), devtools_web_view->y());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

// TODO(https://crbug.com/1020758): Flaky on Linux.
#if defined(OS_LINUX)
#define MAYBE_FindBarBoundingBoxLocationBar \
  DISABLED_FindBarBoundingBoxLocationBar
#else
#define MAYBE_FindBarBoundingBoxLocationBar FindBarBoundingBoxLocationBar
#endif
// Test the find bar's bounding box when the location bar is visible.
TEST_F(BrowserViewTest, MAYBE_FindBarBoundingBoxLocationBar) {
  ASSERT_FALSE(base::i18n::IsRTL());
  const views::View* location_bar = browser_view()->GetLocationBarView();
  const views::View* contents_container =
      browser_view()->GetContentsContainerForTest();

  // Make sure we are testing the case where the location bar is visible.
  EXPECT_TRUE(location_bar->GetVisible());
  const gfx::Rect find_bar_bounds = browser_view()->GetFindBarBoundingBox();
  const gfx::Rect location_bar_bounds =
      location_bar->ConvertRectToWidget(location_bar->GetLocalBounds());
  const gfx::Rect contents_bounds = contents_container->ConvertRectToWidget(
      contents_container->GetLocalBounds());

  const gfx::Rect target(
      location_bar_bounds.x(), location_bar_bounds.bottom(),
      location_bar_bounds.width(),
      contents_bounds.bottom() - location_bar_bounds.bottom());
  EXPECT_EQ(target.ToString(), find_bar_bounds.ToString());
}

// Test the find bar's bounding box when the location bar is not visible.
TEST_F(BrowserViewTest, FindBarBoundingBoxNoLocationBar) {
  ASSERT_FALSE(base::i18n::IsRTL());
  const views::View* location_bar = browser_view()->GetLocationBarView();
  const views::View* contents_container =
      browser_view()->GetContentsContainerForTest();

  // Make sure we are testing the case where the location bar is absent.
  browser_view()->GetLocationBarView()->SetVisible(false);
  EXPECT_FALSE(location_bar->GetVisible());
  const gfx::Rect find_bar_bounds = browser_view()->GetFindBarBoundingBox();
  gfx::Rect contents_bounds = contents_container->ConvertRectToWidget(
      contents_container->GetLocalBounds());
  contents_bounds.Inset(0, 0, gfx::scrollbar_size(), 0);

  EXPECT_EQ(contents_bounds.ToString(), find_bar_bounds.ToString());
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !defined(OS_MACOSX)
// Test that repeated accelerators are processed or ignored depending on the
// commands that they refer to. The behavior for different commands is dictated
// by IsCommandRepeatable() in chrome/browser/ui/views/accelerator_table.h.
TEST_F(BrowserViewTest, DISABLED_RepeatedAccelerators) {
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
// TODO(https://crbug.com/1000251): Flaky on Linux.
#if defined(OS_LINUX)
#define MAYBE_BookmarkBarInvisibleOnShutdown \
  DISABLED_BookmarkBarInvisibleOnShutdown
#else
#define MAYBE_BookmarkBarInvisibleOnShutdown BookmarkBarInvisibleOnShutdown
#endif
TEST_F(BrowserViewTest, MAYBE_BookmarkBarInvisibleOnShutdown) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  Browser* browser = browser_view()->browser();
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());

  AddTab(browser, GURL("about:blank"));
  EXPECT_EQ(1, tab_strip_model->count());

  BookmarkBarView* bookmark_bar = browser_view()->GetBookmarkBarView();
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->GetVisible());

  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0);
  EXPECT_EQ(0, tab_strip_model->count());
  EXPECT_FALSE(bookmark_bar->GetVisible());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

TEST_F(BrowserViewTest, DISABLED_AccessibleWindowTitle) {
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
  BrowserViewHostedAppTest()
      : TestWithBrowserView(Browser::TYPE_POPUP,
                            BrowserWithTestWindowTest::HostedApp()) {}
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
  EXPECT_FALSE(browser_view()->tabstrip()->GetVisible());
  EXPECT_FALSE(browser_view()->toolbar()->GetVisible());
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

using BrowserViewWindowTypeTest = BrowserWithTestWindowTest;

TEST_F(BrowserViewWindowTypeTest, TestWindowIsNotReturned) {
  // Check that BrowserView::GetBrowserViewForBrowser does not return a
  // non-BrowserView BrowserWindow instance - in this case, a TestBrowserWindow.
  EXPECT_NE(nullptr, browser()->window());
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(browser()));
}
