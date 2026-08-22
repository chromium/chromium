// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <memory>
#include <string_view>

#include "base/byte_size.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "components/version_info/channel.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/recently_audible_helper.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#endif

namespace {

// Tab strip bounds depend on the window frame sizes.
gfx::Point ExpectedTabStripRegionOrigin(BrowserView* browser_view) {
  auto* const frame = browser_view->browser_widget()->GetFrameView();
  const auto params = frame->GetBrowserLayoutParams();
  gfx::Point tabstrip_region_origin = params.visual_client_area.origin();
  tabstrip_region_origin.Offset(
      params.leading_exclusion.ContentWithPadding().width(), 0);
  views::View::ConvertPointToTarget(frame, browser_view,
                                    &tabstrip_region_origin);
  return tabstrip_region_origin;
}

}  // namespace

class BrowserViewTest : public TestWithBrowserView {
 public:
  BrowserViewTest() = default;

  BrowserViewTest(const BrowserViewTest&) = delete;
  BrowserViewTest& operator=(const BrowserViewTest&) = delete;

  ~BrowserViewTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        PinnedToolbarActionsModelFactory::GetInstance(),
        base::BindRepeating(&BrowserViewTest::BuildPinnedToolbarActionsModel));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildPinnedToolbarActionsModel(
      content::BrowserContext* context) {
    return std::make_unique<PinnedToolbarActionsModel>(
        Profile::FromBrowserContext(context));
  }
};

namespace {
// A thin wrapper around `Browser` to ensure that it's destructed in the right
// order.
class ScopedBrowser {
 public:
  explicit ScopedBrowser(Profile* profile) {
    BrowserWindowCreateParams params(profile, true);
    browser_ = DeprecatedCreateOwnedBrowserWindowForTesting(std::move(params));
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser_.get());
  }
  ScopedBrowser(const ScopedBrowser&) = delete;
  ScopedBrowser& operator=(const ScopedBrowser&) = delete;
  ~ScopedBrowser() {
    browser()->tab_strip_model()->CloseAllTabs();
    browser_view_.ExtractAsDangling()->GetWidget()->CloseNow();
    content::RunAllTasksUntilIdle();
  }

  Browser* browser() { return browser_.get(); }

 private:
  std::unique_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_;
};
}  // namespace

// TODO(crbug.com/326199292): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_UpdateActiveBrowser DISABLED_UpdateActiveBrowser
#else
#define MAYBE_UpdateActiveBrowser UpdateActiveBrowser
#endif

// Test that calling `BrowserView::Activate()` or `BrowserView::Show()` sets
// the last active browser synchronously.
TEST_F(BrowserViewTest, MAYBE_UpdateActiveBrowser) {
  // On platforms like Ash-Chrome, for `BrowserView::Activate()` to actually
  // activate the browser, it has to be made visible first. Thus
  // `BrowserView::Show()` has to be called first.
  ScopedBrowser scoped_browser(profile());
  Browser* browser2 = scoped_browser.browser();
  EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2->GetWindow()->Show();
  EXPECT_EQ(browser2, GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser()->GetWindow()->Show();
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2->GetWindow()->Activate();
  EXPECT_EQ(browser2, GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser()->GetWindow()->Activate();
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2 = nullptr;
}

// Test layout of the top-of-window UI.
TEST_F(BrowserViewTest, DISABLED_BrowserViewLayout) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  // |browser_view_| owns the Browser, not the test class.
  Browser* browser = browser_view()->browser();
  TopContainerView* top_container = browser_view()->top_container();
  TabStrip* tabstrip = browser_view()->horizontal_tab_strip_for_testing();
  views::View* tabstrip_region =
      browser_view()->horizontal_tab_strip_for_testing()->parent();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::View* contents_container = browser_view()->contents_container();
  views::WebView* contents_web_view = browser_view()->contents_web_view();
  views::WebView* devtools_web_view =
      browser_view()->GetActiveContentsContainerView()->devtools_web_view();

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
                GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap),
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
  EXPECT_EQ(GetLayoutConstant(LayoutConstant::kBookmarkBarHeight),
            bookmark_bar->GetMinimumSize().height());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->GetVisible());
  chrome::ExecuteCommand(browser, IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->GetVisible());

  // The NTP should be treated the same as any other page.
  NavigateAndCommitActiveTabWithTitle(
      browser, chrome::ChromeUINewTabURLAsGURL(), std::u16string());
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
                GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap),
            bookmark_bar->y());
  EXPECT_EQ(bookmark_bar->height() + bookmark_bar->y(),
            contents_container->y());
  EXPECT_EQ(contents_web_view->y(), devtools_web_view->y());

  BookmarkBarView::DisableAnimationsForTesting(false);
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !BUILDFLAG(IS_MAC)
// Test that repeated accelerators are processed or ignored depending on the
// commands that they refer to. The behavior for different commands is dictated
// by IsCommandRepeatable() in chrome/browser/ui/accelerator_table.h.
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
#endif  // !BUILDFLAG(IS_MAC)

using BrowserViewWindowTypeTest = BrowserWithTestWindowTest;

TEST_F(BrowserViewWindowTypeTest, TestWindowIsNotReturned) {
  // Check that BrowserView::GetBrowserViewForBrowser does not return a
  // non-BrowserView BrowserWindow instance - in this case, a TestBrowserWindow.
  EXPECT_NE(nullptr, BrowserWindow::FromBrowser(browser()));
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(browser()));
}
