// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <memory>
#include <string_view>

#include "base/byte_count.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
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
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/recently_audible_helper.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#endif

namespace {

// Tab strip bounds depend on the window frame sizes.
gfx::Point ExpectedTabStripRegionOrigin(BrowserView* browser_view) {
  gfx::Rect tabstrip_bounds(
      browser_view->browser_widget()
          ->GetFrameView()
          ->GetBoundsForTabStripRegion(
              browser_view->tab_strip_view()->GetMinimumSize()));
  gfx::Point tabstrip_region_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToTarget(browser_view->parent(), browser_view,
                                    &tabstrip_region_origin);
  return tabstrip_region_origin;
}

// Helper function to take a prefix and suffix and insert the browser name (like
// "Chromium" or "Google Chrome") in the middle.
std::u16string SubBrowserName(std::u16string_view prefix,
                              std::u16string_view suffix) {
  return base::StrCat(
      {prefix, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), suffix});
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

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // The window is owned by the native widget, not the test class.
  EXPECT_FALSE(window());

  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->GetTabStripVisible());
  EXPECT_FALSE(browser_view()->GetIncognito());
  EXPECT_FALSE(browser_view()->GetGuestSession());
  EXPECT_TRUE(browser_view()->GetIsNormalType());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarAnimating());

  // Test action item creation.
  BrowserActions* browser_actions = browser()->browser_actions();

  ASSERT_NE(browser_actions->root_action_item(), nullptr);
  EXPECT_GE(
      browser_actions->root_action_item()->GetChildren().children().size(),
      1UL);

  actions::ActionItemVector actions;
  auto& manager = actions::ActionManager::GetForTesting();
  manager.GetActions(actions);

  actions::ActionItem* customize_chrome_action = manager.FindAction(
      kActionSidePanelShowCustomizeChrome, browser_actions->root_action_item());
  EXPECT_EQ(customize_chrome_action->GetText(),
            l10n_util::GetStringUTF16(IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE));
  EXPECT_EQ(customize_chrome_action->GetImage(),
            ui::ImageModel::FromVectorIcon(vector_icons::kEditChromeRefreshIcon,
                                           ui::kColorIcon));
  EXPECT_EQ(customize_chrome_action->GetEnabled(), true);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BrowserViewTest, OnTaskLockedBrowserView) {
  ASSERT_TRUE(browser_view()->browser());
  browser_view()->browser()->SetLockedForOnTask(true);
  EXPECT_FALSE(browser_view()->CanMinimize());
  EXPECT_FALSE(browser_view()->ShouldShowCloseButton());
}

TEST_F(BrowserViewTest, OnTaskUnlockedBrowserView) {
  ASSERT_TRUE(browser_view()->browser());
  browser_view()->browser()->SetLockedForOnTask(false);
  EXPECT_TRUE(browser_view()->CanMinimize());
  EXPECT_TRUE(browser_view()->ShouldShowCloseButton());
}
#endif

namespace {
// A thin wrapper around `Browser` to ensure that it's destructed in the right
// order.
class ScopedBrowser {
 public:
  explicit ScopedBrowser(Profile* profile) {
    Browser::CreateParams params(profile, true);
    browser_view_ =
        BrowserView::GetBrowserViewForBrowser(Browser::Create(params));
  }
  ScopedBrowser(const ScopedBrowser&) = delete;
  ScopedBrowser& operator=(const ScopedBrowser&) = delete;
  ~ScopedBrowser() {
    browser_view_->browser()->tab_strip_model()->CloseAllTabs();
    browser_view_.ExtractAsDangling()->GetWidget()->CloseNow();
    content::RunAllTasksUntilIdle();
  }

  Browser* browser() { return browser_view_->browser(); }

 private:
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
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2->window()->Show();
  EXPECT_EQ(browser2, GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser()->window()->Show();
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2->window()->Activate();
  EXPECT_EQ(browser2, GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser()->window()->Activate();
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  browser2 = nullptr;
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
  NavigateAndCommitActiveTabWithTitle(browser, GURL(chrome::kChromeUINewTabURL),
                                      std::u16string());
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

// TODO(crbug.com/40656637): Flaky on Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FindBarBoundingBoxLocationBar \
  DISABLED_FindBarBoundingBoxLocationBar
#else
#define MAYBE_FindBarBoundingBoxLocationBar FindBarBoundingBoxLocationBar
#endif
// Test the find bar's bounding box when the location bar is visible.
TEST_F(BrowserViewTest, MAYBE_FindBarBoundingBoxLocationBar) {
  ASSERT_FALSE(base::i18n::IsRTL());
  const views::View* location_bar = browser_view()->GetLocationBarView();
  const views::View* contents_container = browser_view()->contents_container();

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
  const views::View* contents_container = browser_view()->contents_container();

  // Make sure we are testing the case where the location bar is absent.
  browser_view()->GetLocationBarView()->SetVisible(false);
  EXPECT_FALSE(location_bar->GetVisible());
  const gfx::Rect find_bar_bounds = browser_view()->GetFindBarBoundingBox();
  gfx::Rect contents_bounds = contents_container->ConvertRectToWidget(
      contents_container->GetLocalBounds());
  contents_bounds.Inset(gfx::Insets::TLBR(0, 0, 0, gfx::scrollbar_size()));

  EXPECT_EQ(contents_bounds.ToString(), find_bar_bounds.ToString());
}

// Tests that a browser window is correctly associated to a WebContents that
// belongs to that window's UI hierarchy.
TEST_F(BrowserViewTest, FindBrowserWindowWithWebContents) {
  auto web_view = std::make_unique<views::WebView>(browser()->profile());
  ASSERT_NE(nullptr, web_view->GetWebContents());

  // If the web contents does not belong browser's UI hierarchy there should not
  // be a browser window associated with the contents.
  EXPECT_EQ(nullptr, BrowserWindow::FindBrowserWindowWithWebContents(
                         web_view->GetWebContents()));

  // After adding the web contents to the browser's UI hierarchy the browser
  // window should be correctly associated with the contents.
  auto* web_view_ptr = browser_view()->AddChildView(std::move(web_view));
  EXPECT_EQ(browser()->window(),
            BrowserWindow::FindBrowserWindowWithWebContents(
                web_view_ptr->GetWebContents()));

  // Removing the web contents from the browser's UI hierarchy should
  // disassociate it with the browser window.
  web_view = browser_view()->RemoveChildViewT(web_view_ptr);
  EXPECT_EQ(nullptr, BrowserWindow::FindBrowserWindowWithWebContents(
                         web_view->GetWebContents()));
}

// Tests that tab contents are correctly associated with their browser window,
// even when non-active.
TEST_F(BrowserViewTest, FindBrowserWindowWithWebContentsTabSwitch) {
  AddTab(browser_view()->browser(), GURL("about:blank"));
  content::WebContents* original_active_contents =
      browser_view()->GetActiveWebContents();
  EXPECT_EQ(browser()->window(),
            BrowserWindow::FindBrowserWindowWithWebContents(
                original_active_contents));

  // Inactive tabs (aka tabs with their web contents not currently embedded in
  // the browser's ContentWebView) should still be associated with their hosting
  // browser window.
  AddTab(browser_view()->browser(), GURL("about:blank"));
  content::WebContents* new_active_contents =
      browser_view()->GetActiveWebContents();
  EXPECT_NE(original_active_contents, browser_view()->GetActiveWebContents());
  EXPECT_EQ(new_active_contents, browser_view()->GetActiveWebContents());
  EXPECT_EQ(browser()->window(),
            BrowserWindow::FindBrowserWindowWithWebContents(
                original_active_contents));
  EXPECT_EQ(
      browser()->window(),
      BrowserWindow::FindBrowserWindowWithWebContents(new_active_contents));
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

TEST_F(BrowserViewTest, UpdateWindowTitle) {
  AddTab(browser(), GURL("about:blank"));
  AddTab(browser(), GURL("about:blank"));
  std::string user_title1 = "Test Title";
  browser()->SetWindowUserTitle(user_title1);
  auto window_title = browser_view()->GetAccessibleWindowTitle();
  EXPECT_EQ(base::UTF8ToUTF16(user_title1),
            window_title.substr(0, user_title1.size()));

  std::string user_title2 = "Test Title 2";
  browser()->SetWindowUserTitle(user_title2);
  window_title = browser_view()->GetAccessibleWindowTitle();
  EXPECT_EQ(base::UTF8ToUTF16(user_title2),
            window_title.substr(0, user_title2.size()));
}

TEST_F(BrowserViewTest, WindowTitleOmitsLowMemoryUsage) {
  scoped_refptr<TabResourceUsage> tab_resource_usage_ =
      base::MakeRefCounted<TabResourceUsage>();
  tab_resource_usage_->SetMemoryUsage(base::ByteCount(100));

  TabRendererData memory_usage;
  memory_usage.tab_resource_usage = tab_resource_usage_;

  AddTab(browser(), GURL("about:blank"));
  Tab* const tab = browser_view()->tabstrip()->tab_at(0);
  tab->SetData(std::move(memory_usage));

  // Expect that low memory usage isn't in the window title.
  EXPECT_EQ(SubBrowserName(u"about:blank - ", u""),
            browser_view()->GetAccessibleWindowTitle());
  base::ByteCount memory_used =
      TabResourceUsage::kHighMemoryUsageThreshold + base::ByteCount(1);
  tab_resource_usage_->SetMemoryUsage(memory_used);

  // Expect that high memory usage is in the window title.
  EXPECT_TRUE(browser_view()->GetAccessibleWindowTitle().find(
                  u"High memory usage") != std::string::npos);
}

#if BUILDFLAG(IS_MAC)
// Tests that audio playing state is reflected in the "Window" menu on Mac.
TEST_F(BrowserViewTest, TitleAudioIndicators) {
  std::u16string playing_icon = u"\U0001F50A";
  std::u16string muted_icon = u"\U0001F507";

  AddTab(browser_view()->browser(), GURL("about:blank"));
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  RecentlyAudibleHelper* audible_helper =
      RecentlyAudibleHelper::FromWebContents(contents);

  audible_helper->SetNotRecentlyAudibleForTesting();
  EXPECT_EQ(browser_view()->GetWindowTitle().find(playing_icon),
            std::u16string::npos);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(muted_icon),
            std::u16string::npos);

  audible_helper->SetCurrentlyAudibleForTesting();
  EXPECT_NE(browser_view()->GetWindowTitle().find(playing_icon),
            std::u16string::npos);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(muted_icon),
            std::u16string::npos);

  audible_helper->SetRecentlyAudibleForTesting();
  contents->SetAudioMuted(true);
  EXPECT_EQ(browser_view()->GetWindowTitle().find(playing_icon),
            std::u16string::npos);
  EXPECT_NE(browser_view()->GetWindowTitle().find(muted_icon),
            std::u16string::npos);
}
#endif

TEST_F(BrowserViewTest, RotatePaneFocusFromView) {
  auto dialog_model = ui::DialogModel::Builder()
                          .SetTitle(u"test")
                          .SetIsAlertDialog()
                          .AddOkButton(base::DoNothing())
                          .Build();
  auto* anchor = browser_view()->toolbar_button_provider()->GetAppMenuButton();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor, views::BubbleBorder::TOP_RIGHT);
  auto* bubble_ptr = bubble.get();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();

  // OK button cannot be retrieved until CreateBubble has been called.
  auto* ok_button = bubble_ptr->GetOkButton();

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->SetKeyboardAccessible(true);

  // Initial rotation should return a "rotated" result.
  EXPECT_TRUE(browser_view()->RotatePaneFocusFromView(nullptr, true, true));
  EXPECT_EQ(ok_button, focus_manager->GetStoredFocusView());

  // Next rotation should not return a "rotated" result and should not change
  // the focus.
  EXPECT_FALSE(browser_view()->RotatePaneFocusFromView(nullptr, true, false));
  EXPECT_EQ(ok_button, focus_manager->GetStoredFocusView());
}

TEST_F(BrowserViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  browser_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kClient);

  ui::AXNodeData root_view_data;
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      browser_view()->GetAccessibleWindowTitle());
}

TEST_F(BrowserViewTest, UpdateAccessibleURL) {
  const GURL before_url(u"data:text/html,before");
  AddTab(browser(), before_url);

  ui::AXNodeData node_data;
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            before_url);

  const GURL after_url(u"data:text/html,after");
  AddTab(browser(), after_url);
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            after_url);
}

TEST_F(BrowserViewTest, UpdateAccessibleURLOnTabSelection) {
  // Create two tabs with different URLs
  const GURL url1(u"data:text/html,tab1");
  const GURL url2(u"data:text/html,tab2");

  AddTab(browser(), url1);
  AddTab(browser(), url2);

  // Initially, the second tab should be active (most recently added)
  // Note: AddTab inserts at index 0, so tab2 is at index 0, tab1 is at index 1
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  ui::AXNodeData node_data;
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            url2);

  // Switch to the first tab (tab1 at index 1) by changing selection
  browser()->tab_strip_model()->SelectTabAt(1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);

  // Verify that the accessible URL was updated due to tab selection change
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            url1);

  // Switch back to the second tab (tab2 at index 0)
  browser()->tab_strip_model()->SelectTabAt(0);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  // Verify that the accessible URL was updated again
  browser_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kUrl),
            url2);
}

//  Macs do not have fullscreen policy.
#if !BUILDFLAG(IS_MAC)

TEST_F(BrowserViewTest, CanFullscreenPolicyWatcher) {
  auto* fullscreen_pref_path = prefs::kFullscreenAllowed;
  EXPECT_TRUE(browser_view()->CanFullscreen());

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(fullscreen_pref_path,
                                                       false);
  EXPECT_FALSE(browser_view()->CanFullscreen());

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(fullscreen_pref_path,
                                                       true);
  EXPECT_TRUE(browser_view()->CanFullscreen());
}

class BrowserViewPipTest : public TestWithBrowserView {
 public:
  BrowserViewPipTest()
      : TestWithBrowserView(Browser::TYPE_PICTURE_IN_PICTURE) {}

  BrowserViewPipTest(const BrowserViewPipTest&) = delete;
  BrowserViewPipTest& operator=(const BrowserViewPipTest&) = delete;

  ~BrowserViewPipTest() override = default;
};

// Pip is used to test reverting back to not allowed to fullscreen state.
TEST_F(BrowserViewPipTest, CanFullscreenPolicyDoesNotEnableFullscreen) {
  auto* fullscreen_pref_path = prefs::kFullscreenAllowed;
  EXPECT_FALSE(browser_view()->CanFullscreen());

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(fullscreen_pref_path,
                                                       false);
  EXPECT_FALSE(browser_view()->CanFullscreen());

  // This should have no effect, because pip is not allowed to enter fullscreen.
  browser_view()->GetProfile()->GetPrefs()->SetBoolean(fullscreen_pref_path,
                                                       true);
  EXPECT_FALSE(browser_view()->CanFullscreen());
}

#endif  // !BUILDFLAG(IS_MAC)

class BrowserViewHostedAppTest : public TestWithBrowserView {
 public:
  BrowserViewHostedAppTest()
      : TestWithBrowserView(Browser::TYPE_POPUP,
                            BrowserWithTestWindowTest::HostedApp()) {}

  BrowserViewHostedAppTest(const BrowserViewHostedAppTest&) = delete;
  BrowserViewHostedAppTest& operator=(const BrowserViewHostedAppTest&) = delete;

  ~BrowserViewHostedAppTest() override = default;
};

// Test basic layout for hosted apps.
TEST_F(BrowserViewHostedAppTest, Layout) {
  // Add a tab because the browser starts out without any tabs at all.
  AddTab(browser(), GURL("about:blank"));

  const int contents_container_y = browser_view()->contents_container()->y();

  // The tabstrip, toolbar and bookmark bar should not be visible for hosted
  // apps.
  EXPECT_FALSE(browser_view()->tabstrip()->GetVisible());
  EXPECT_FALSE(browser_view()->toolbar()->GetVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());

  gfx::Point header_offset;
  views::View::ConvertPointToTarget(
      browser_view(),
      browser_view()->browser_widget()->non_client_view()->frame_view(),
      &header_offset);

  // The position of the bottom of the header (the bar with the window
  // controls) in the coordinates of BrowserView.
  const int top_inset =
      browser_view()->browser_widget()->GetFrameView()->GetTopInset(false);
  const int bottom_of_header = top_inset - header_offset.y();

  // The web contents should be flush with the bottom of the header.
  EXPECT_EQ(bottom_of_header, contents_container_y);

  // The find bar should butt against the 1px header/web-contents separator at
  // the bottom of the header.
  EXPECT_EQ(top_inset, browser_view()->GetFindBarBoundingBox().y());
}

using BrowserViewWindowTypeTest = BrowserWithTestWindowTest;

TEST_F(BrowserViewWindowTypeTest, TestWindowIsNotReturned) {
  // Check that BrowserView::GetBrowserViewForBrowser does not return a
  // non-BrowserView BrowserWindow instance - in this case, a TestBrowserWindow.
  EXPECT_NE(nullptr, browser()->window());
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(browser()));
}

// Tests Feature to ensure that the loading animation is not rendered after the
// window changes to hidden.
TEST_F(TestWithBrowserView, LoadingAnimationNotRenderedWhenWindowHidden) {
  TabActivitySimulator tab_activity_simulator;
  content::WebContents* web_contents =
      tab_activity_simulator.AddWebContentsAndNavigate(
          browser()->tab_strip_model(), GURL("about:blank"));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("about:blank"), web_contents);
  navigation->SetKeepLoading(true);

  browser_view()->browser_widget()->Show();

  EXPECT_TRUE(browser()->tab_strip_model()->TabsNeedLoadingUI());
  EXPECT_TRUE(browser_view()->IsLoadingAnimationRunning());

  browser_view()->browser_widget()->Hide();

  EXPECT_TRUE(browser()->tab_strip_model()->TabsNeedLoadingUI());
  EXPECT_FALSE(browser_view()->IsLoadingAnimationRunning());
}
