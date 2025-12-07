// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <memory>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/test_util.h"
#endif
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_test_helper.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point.h"
#include "url/url_constants.h"

#if defined(USE_AURA)
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // USE_AURA

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

class BrowserViewTest : public InProcessBrowserTest {
 public:
  BrowserViewTest() : devtools_(nullptr) {
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

  BrowserViewTest(const BrowserViewTest&) = delete;
  BrowserViewTest& operator=(const BrowserViewTest&) = delete;

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  views::WebView* devtools_web_view() {
    return browser_view()
        ->GetActiveContentsContainerView()
        ->devtools_web_view();
  }

  views::WebView* contents_web_view() {
    return browser_view()->contents_web_view();
  }

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ScrimView* active_contents_scrim_view() {
    return browser_view()
        ->GetActiveContentsContainerView()
        ->contents_scrim_view();
  }

  ContentsContainerView* active_contents_container_view() {
    return browser_view()
        ->multi_contents_view()
        ->GetActiveContentsContainerView();
  }

  ContentsContainerView* inactive_contents_container_view() {
    return browser_view()
        ->multi_contents_view()
        ->GetInactiveContentsContainerView();
  }

  void OpenDevToolsWindow(bool docked) {
    devtools_ =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), docked);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(
        devtools_.ExtractAsDangling());
  }

  void SetDevToolsBounds(const gfx::Rect& bounds) {
    DevToolsWindowTesting::Get(devtools_)->SetInspectedPageBounds(bounds);
  }

  raw_ptr<DevToolsWindow> devtools_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class BrowserViewWithoutSideBySideTest : public BrowserViewTest {
 public:
  BrowserViewWithoutSideBySideTest() {
    scoped_feature_list_.InitWithFeatures({}, {features::kSideBySide});
  }

  SidePanel* side_panel() {
    return browser_view()->contents_height_side_panel();
  }

  views::View* side_panel_rounded_corner() {
    return browser_view()->GetSidePanelRoundedCornerForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS)
using BrowserViewChromeOSTest = ChromeOSBrowserUITest;
using BrowserViewChromeOSTestNoWebUiTabStrip =
    WebUiTabStripOverrideTest<false, BrowserViewChromeOSTest>;
#endif

namespace {
// Used to simulate scenario in a crash. When WebContentsDestroyed() is
// invoked updates the navigation state of another tab.
class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  TestWebContentsObserver(content::WebContents* source,
                          content::WebContents* other)
      : content::WebContentsObserver(source), other_(other) {}

  TestWebContentsObserver(const TestWebContentsObserver&) = delete;
  TestWebContentsObserver& operator=(const TestWebContentsObserver&) = delete;

  ~TestWebContentsObserver() override = default;

  void WebContentsDestroyed() override {
    other_->NotifyNavigationStateChanged(static_cast<content::InvalidateTypes>(
        content::INVALIDATE_TYPE_URL | content::INVALIDATE_TYPE_LOAD));
  }

 private:
  raw_ptr<content::WebContents, DanglingUntriaged> other_;
};

// Waits for a different view to claim focus within a widget with the
// specified name.
class TestFocusChangeWaiter : public views::FocusChangeListener {
 public:
  TestFocusChangeWaiter(views::FocusManager* focus_manager,
                        const std::string& expected_widget_name)
      : focus_manager_(focus_manager),
        expected_widget_name_(expected_widget_name) {
    if (auto* current_focused_view = focus_manager->GetFocusedView()) {
      previous_view_id_ = current_focused_view->GetID();
    } else {
      previous_view_id_ = -1;
    }
    focus_manager_->AddFocusChangeListener(this);
  }

  TestFocusChangeWaiter(const TestFocusChangeWaiter&) = delete;
  TestFocusChangeWaiter& operator=(const TestFocusChangeWaiter&) = delete;
  ~TestFocusChangeWaiter() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // views::FocusChangeListener:
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (focused_now && focused_now->GetID() != previous_view_id_) {
      views::Widget* widget = focused_now->GetWidget();
      if (widget && widget->GetName() == expected_widget_name_) {
        run_loop_.Quit();
      }
    }
  }

  raw_ptr<views::FocusManager> focus_manager_;
  base::RunLoop run_loop_;
  int previous_view_id_;
  std::string expected_widget_name_;
  base::WeakPtrFactory<TestFocusChangeWaiter> weak_factory_{this};
};

class TestTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit TestTabModalConfirmDialogDelegate(content::WebContents* contents)
      : TabModalConfirmDialogDelegate(contents) {}

  TestTabModalConfirmDialogDelegate(const TestTabModalConfirmDialogDelegate&) =
      delete;
  TestTabModalConfirmDialogDelegate& operator=(
      const TestTabModalConfirmDialogDelegate&) = delete;

  std::u16string GetTitle() override { return std::u16string(u"Dialog Title"); }
  std::u16string GetDialogMessage() override { return std::u16string(); }
};
}  // namespace

// Verifies don't crash when CloseNow() is invoked with two tabs in a browser.
// Additionally when one of the tabs is destroyed NotifyNavigationStateChanged()
// is invoked on the other.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, CloseWithTabs) {
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(browser2, GURL(), -1, true);
  chrome::AddTabAt(browser2, GURL(), -1, true);
  TestWebContentsObserver observer(
      browser2->tab_strip_model()->GetWebContentsAt(0),
      browser2->tab_strip_model()->GetWebContentsAt(1));
  BrowserView::GetBrowserViewForBrowser(browser2)->GetWidget()->CloseNow();
}

// Same as CloseWithTabs, but activates the first tab, which is the first tab
// BrowserView will destroy.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, CloseWithTabsStartWithActive) {
  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  chrome::AddTabAt(browser2, GURL(), -1, true);
  chrome::AddTabAt(browser2, GURL(), -1, true);
  browser2->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  TestWebContentsObserver observer(
      browser2->tab_strip_model()->GetWebContentsAt(0),
      browser2->tab_strip_model()->GetWebContentsAt(1));
  BrowserView::GetBrowserViewForBrowser(browser2)->GetWidget()->CloseNow();
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserViewTest, OnTaskLockedBrowserView) {
  browser()->SetLockedForOnTask(true);
  EXPECT_FALSE(browser_view()->CanMinimize());
  EXPECT_FALSE(browser_view()->ShouldShowCloseButton());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, OnTaskUnlockedBrowserView) {
  browser()->SetLockedForOnTask(false);
  EXPECT_TRUE(browser_view()->CanMinimize());
  EXPECT_TRUE(browser_view()->ShouldShowCloseButton());
}
#endif

// Verifies that page and devtools WebViews are being correctly laid out
// when DevTools is opened/closed/updated while docked.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, DevToolsDockedUpdatesBrowserWindow) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support getting/setting window position in global
  // screen coordinates. So this test is not applicable.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  gfx::Rect full_bounds = active_contents_container_view()->GetLocalBounds();
  gfx::Rect small_bounds(10, 20, 30, 40);

  browser_view()->UpdateDevTools(active_web_contents());
  EXPECT_FALSE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(full_bounds, contents_web_view()->bounds());

  // Docked.
  OpenDevToolsWindow(true);
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());

  SetDevToolsBounds(small_bounds);
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(small_bounds, contents_web_view()->bounds());

  browser_view()->UpdateDevTools(active_web_contents());
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(small_bounds, contents_web_view()->bounds());

  CloseDevToolsWindow();
  EXPECT_FALSE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(full_bounds, contents_web_view()->bounds());

  browser_view()->UpdateDevTools(active_web_contents());
  EXPECT_FALSE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(full_bounds, contents_web_view()->bounds());
}

// Verifies that page and devtools WebViews are being correctly laid out
// when DevTools is opened/closed/updated while undocked.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, DevToolsUndockedUpdatesBrowserWindow) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support getting/setting window position in global
  // screen coordinates. So this test is not applicable.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  gfx::Rect full_bounds = active_contents_container_view()->GetLocalBounds();
  gfx::Rect small_bounds(10, 20, 30, 40);

  OpenDevToolsWindow(false);
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());

  SetDevToolsBounds(small_bounds);
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(small_bounds, contents_web_view()->bounds());

  browser_view()->UpdateDevTools(active_web_contents());
  EXPECT_TRUE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(small_bounds, contents_web_view()->bounds());

  CloseDevToolsWindow();
  EXPECT_FALSE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(full_bounds, contents_web_view()->bounds());

  browser_view()->UpdateDevTools(active_web_contents());
  EXPECT_FALSE(devtools_web_view()->web_contents());
  EXPECT_EQ(full_bounds, devtools_web_view()->bounds());
  EXPECT_EQ(full_bounds, contents_web_view()->bounds());
}

void SetDevToolsWindowSizePrefs(Browser* browser,
                                int left,
                                int right,
                                int top,
                                int bottom) {
  PrefService* prefs = browser->GetProfile()->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kAppWindowPlacement);
  base::Value::Dict& wp_prefs = update.Get();
  base::Value::Dict dev_tools_defaults;
  dev_tools_defaults.Set("left", left);
  dev_tools_defaults.Set("right", right);
  dev_tools_defaults.Set("top", top);
  dev_tools_defaults.Set("bottom", bottom);
  dev_tools_defaults.Set("maximized", false);
  dev_tools_defaults.Set("always_on_top", false);
  wp_prefs.Set(DevToolsWindow::kDevToolsApp, std::move(dev_tools_defaults));
}

const base::Value::Dict& GetDevToolsWindowSizePrefs(Browser* browser) {
  PrefService* prefs = browser->GetProfile()->GetPrefs();
  return prefs->GetDict(prefs::kAppWindowPlacement)
      .Find(DevToolsWindow::kDevToolsApp)
      ->GetDict();
}

auto HasDimensions(int left, int right, int top, int bottom) {
  return base::test::DictionaryHasValues(base::Value::Dict()
                                             .Set("left", left)
                                             .Set("right", right)
                                             .Set("top", top)
                                             .Set("bottom", bottom));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, DevToolsWindowDefaultSize) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support getting/setting window position in global
  // screen coordinates. So this test is not applicable.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  // Starting DevTools the first time sets the window size to the default.
  OpenDevToolsWindow(false);
  CloseDevToolsWindow();
  EXPECT_THAT(GetDevToolsWindowSizePrefs(browser()),
              HasDimensions(100, 740, 100, 740));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, DevToolsWindowKeepsSize) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support getting/setting window position in global
  // screen coordinates. So this test is not applicable.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  // Setting reasonable size prefs does not change the prefs.
  SetDevToolsWindowSizePrefs(browser(), 123, 567, 234, 678);
  EXPECT_THAT(GetDevToolsWindowSizePrefs(browser()),
              HasDimensions(123, 567, 234, 678));
  OpenDevToolsWindow(false);
  CloseDevToolsWindow();
  EXPECT_THAT(GetDevToolsWindowSizePrefs(browser()),
              HasDimensions(123, 567, 234, 678));
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, DevToolsWindowResetsSize) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support getting/setting window position in global
  // screen coordinates. So this test is not applicable.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  // Setting unreasonably small size prefs resets the prefs.
  SetDevToolsWindowSizePrefs(browser(), 121, 232, 343, 454);
  EXPECT_THAT(GetDevToolsWindowSizePrefs(browser()),
              HasDimensions(121, 232, 343, 454));
  OpenDevToolsWindow(false);
  CloseDevToolsWindow();
  EXPECT_THAT(GetDevToolsWindowSizePrefs(browser()),
              HasDimensions(100, 740, 100, 740));
}

// Verifies that the side panel's rounded corner is being correctly layed out.
IN_PROC_BROWSER_TEST_F(BrowserViewWithoutSideBySideTest,
                       SidePanelRoundedCornerLayout) {
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->SetNoDelaysForTesting(true);
  side_panel_ui->Show(SidePanelEntry::Id::kBookmarks);
  if (base::FeatureList::IsEnabled(features::kTabbedBrowserUseNewLayout)) {
    browser()->GetBrowserView().GetWidget()->LayoutRootViewIfNecessary();
  }
  EXPECT_EQ(side_panel()->bounds().x(),
            side_panel_rounded_corner()->bounds().right());
  EXPECT_EQ(side_panel()->bounds().y(),
            side_panel_rounded_corner()->bounds().y());
}

class BookmarkBarViewObserverImpl : public BookmarkBarViewObserver {
 public:
  BookmarkBarViewObserverImpl() = default;

  BookmarkBarViewObserverImpl(const BookmarkBarViewObserverImpl&) = delete;
  BookmarkBarViewObserverImpl& operator=(const BookmarkBarViewObserverImpl&) =
      delete;

  int change_count() const { return change_count_; }
  void clear_change_count() { change_count_ = 0; }

  // BookmarkBarViewObserver:
  void OnBookmarkBarVisibilityChanged() override { change_count_++; }

 private:
  int change_count_ = 0;
};

// Verifies we don't unnecessarily change the visibility of the BookmarkBarView.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, AvoidUnnecessaryVisibilityChanges) {
  // Create two tabs, the first empty and the second the ntp. Make it so the
  // BookmarkBarView isn't shown.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowBookmarkBar, false);
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_tab_url));

  ASSERT_TRUE(browser_view()->bookmark_bar());
  BookmarkBarViewObserverImpl observer;
  BookmarkBarView* bookmark_bar = browser_view()->bookmark_bar();
  bookmark_bar->AddObserver(&observer);
  EXPECT_FALSE(bookmark_bar->GetVisible());

  // Go to empty tab. Bookmark bar should hide.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(bookmark_bar->GetVisible());
  EXPECT_EQ(0, observer.change_count());
  observer.clear_change_count();

  // Go to ntp tab. Bookmark bar should not show.
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(bookmark_bar->GetVisible());
  EXPECT_EQ(0, observer.change_count());
  observer.clear_change_count();

  // Repeat with the bookmark bar always visible.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowBookmarkBar, true);
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(bookmark_bar->GetVisible());
  EXPECT_EQ(1, observer.change_count());
  observer.clear_change_count();

  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(bookmark_bar->GetVisible());
  EXPECT_EQ(0, observer.change_count());
  observer.clear_change_count();

  browser_view()->bookmark_bar()->RemoveObserver(&observer);
}

// Launch the app, navigate to a page with a title, check that the tab title
// is set before load finishes and the throbber state updates when the title
// changes. Regression test for crbug.com/752266
IN_PROC_BROWSER_TEST_F(BrowserViewTest, TitleAndLoadState) {
  const std::u16string test_title(u"Title Of Awesomeness");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(contents, test_title);
  content::TestNavigationObserver navigation_watcher(
      contents, 1, content::MessageLoopRunner::QuitMode::DEFERRED);

  TabStrip* tab_strip = browser_view()->tabstrip();
  // Navigate without blocking.
  const GURL test_url = chrome_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  contents->GetController().LoadURL(test_url, content::Referrer(),
                                    ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(browser()->tab_strip_model()->TabsNeedLoadingUI());
  EXPECT_EQ(TabNetworkState::kWaiting,
            tab_strip->tab_at(0)->data().network_state);
  EXPECT_EQ(test_title, title_watcher.WaitAndGetTitle());
  EXPECT_TRUE(browser()->tab_strip_model()->TabsNeedLoadingUI());
  EXPECT_EQ(TabNetworkState::kLoading,
            tab_strip->tab_at(0)->data().network_state);

  // Now block for the navigation to complete.
  navigation_watcher.Wait();
  EXPECT_FALSE(browser()->tab_strip_model()->TabsNeedLoadingUI());
  EXPECT_EQ(TabNetworkState::kNone, tab_strip->tab_at(0)->data().network_state);
}

// Verifies a tab should show its favicon.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, ShowFaviconInTab) {
  // Opens "chrome://version/" page, which uses default favicon.
  const GURL version_url(chrome::kChromeUIVersionURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), version_url));
  auto* const tab_features =
      browser()->tab_strip_model()->GetActiveTab()->GetTabFeatures();
  auto* const helper = tab_features->tab_ui_helper();
  ASSERT_TRUE(helper);

  const auto favicon = helper->GetFavicon();
  ASSERT_FALSE(favicon.IsEmpty());
}

// On Mac, voiceover treats tab modal dialogs as native windows, so setting an
// accessible title for tab-modal dialogs is not necessary.
#if !BUILDFLAG(IS_MAC)

// Open a tab-modal dialog and check that the accessibility tree only contains
// the dialog.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, GetAccessibleTabModalDialogTree) {
  content::ScopedAccessibilityModeOverride ax_mode_override(
      ui::kAXModeComplete);
  ui::AXPlatformNode* ax_node = ui::AXPlatformNode::FromNativeViewAccessible(
      browser_view()->GetWidget()->GetRootView()->GetNativeViewAccessible());
// We expect this conversion to be safe on Windows, but can't guarantee that it
// is safe on other platforms.
#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(ax_node);
#else
  if (!ax_node) {
    return;
  }
#endif

  // There is no dialog, but the browser UI should be visible. So we expect the
  // browser's reload button and no "OK" button from a dialog.
  EXPECT_NE(ui::AXPlatformNodeTestHelper::FindChildByName(ax_node, "Reload"),
            nullptr);
  EXPECT_EQ(ui::AXPlatformNodeTestHelper::FindChildByName(ax_node, "OK"),
            nullptr);

  content::WebContents* contents = browser_view()->GetActiveWebContents();
  auto delegate = std::make_unique<TestTabModalConfirmDialogDelegate>(contents);

  // `ViewAXPlatformNodeDelegate::GetChildWidgets` expects the following
  // conditions to be met in order to conclude that a tab modal dialog is
  // showing:
  // 1. The dialog is included in `Widget::GetAllOwnedWidgets()`.
  // 2. The currently-focused view is contained in the dialog.
  // Waiting for the dialog to be shown should ensure that the first
  // condition is met. But we also need to wait for the focus to change
  // or the second condition flakily fails.
  TestFocusChangeWaiter focus_waiter(browser_view()->GetFocusManager(),
                                     "MessageBoxView");
  TabModalConfirmDialog::Create(std::move(delegate), contents);
  focus_waiter.Wait();

  // The tab modal dialog should be in the accessibility tree; everything else
  // should be hidden. So we expect an "OK" button and no reload button.
  EXPECT_EQ(ui::AXPlatformNodeTestHelper::FindChildByName(ax_node, "Reload"),
            nullptr);
  EXPECT_NE(ui::AXPlatformNodeTestHelper::FindChildByName(ax_node, "OK"),
            nullptr);
}
#endif  // !BUILDFLAG(IS_MAC)

// Tests that a content area scrim is shown when a tab modal dialog is active.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, ScrimForTabModal) {
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  auto delegate = std::make_unique<TestTabModalConfirmDialogDelegate>(contents);

  // Showing a tab modal dialog will enable the content scrim.
  TabModalConfirmDialog::Create(std::move(delegate), contents);
  EXPECT_TRUE(active_contents_scrim_view()->GetVisible());

  // Goes to a second tab will disable the content scrim.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK));
  EXPECT_FALSE(active_contents_scrim_view()->GetVisible());

  // Switch back to the page that has a modal dialog.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  EXPECT_TRUE(active_contents_scrim_view()->GetVisible());

  // Closing the tab disables the content scrim.
  chrome::CloseWebContents(browser(),
                           browser()->tab_strip_model()->GetActiveWebContents(),
                           /*add_to_history=*/false);
}

// MacOS does not need views window scrim. We use sheet to show window modals
// (-[NSWindow beginSheet:]), which natively draws a scrim.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(BrowserViewTest, ScrimForBrowserWindowModal) {
  auto child_widget_delegate = std::make_unique<views::WidgetDelegate>();
  auto child_widget = std::make_unique<views::Widget>();
  child_widget_delegate->SetModalType(ui::mojom::ModalType::kWindow);
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = child_widget_delegate.get();
  params.parent = browser_view()->GetWidget()->GetNativeView();
  child_widget->Init(std::move(params));

  child_widget->Show();
  EXPECT_TRUE(browser_view()->window_scrim_view()->GetVisible());
  child_widget->Hide();
  EXPECT_FALSE(browser_view()->window_scrim_view()->GetVisible());
  child_widget->Show();
  EXPECT_TRUE(browser_view()->window_scrim_view()->GetVisible());
  // Destroy the child widget, the parent should be notified about child modal
  // visibility change.
  child_widget.reset();
  EXPECT_FALSE(browser_view()->window_scrim_view()->GetVisible());
}
#endif  // !BUILDFLAG(IS_MAC)

// Tests that GetInactiveSplitTabIndex returns correctly with two adjacent
// splits.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, SplitViewActiveIndexTest) {
  // Add enough tabs to create two split views.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  // Add tabs to splits.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  browser()->tab_strip_model()->ActivateTabAt(2);
  browser()->tab_strip_model()->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(browser_view()->multi_contents_view());
  EXPECT_EQ(
      browser_view()->multi_contents_view()->GetActiveContentsView(),
      browser_view()->multi_contents_view()->start_contents_view_for_testing());

  browser()->tab_strip_model()->ActivateTabAt(2);
  EXPECT_EQ(
      browser_view()->multi_contents_view()->GetActiveContentsView(),
      browser_view()->multi_contents_view()->start_contents_view_for_testing());

  browser()->tab_strip_model()->ActivateTabAt(3);
  EXPECT_EQ(
      browser_view()->multi_contents_view()->GetActiveContentsView(),
      browser_view()->multi_contents_view()->end_contents_view_for_testing());
}

// Verifies that page and devtools WebViews are being correctly laid out
// when DevTools is opened/closed/updated while docked.
IN_PROC_BROWSER_TEST_F(BrowserViewTest,
                       DevToolsDockedRemainsOpenInWithFocusInSplit) {
  // Add enough tabs to create two split views.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  // Add tabs to splits.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  browser()->tab_strip_model()->ActivateTabAt(2);
  browser()->tab_strip_model()->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify neither devtools is visible.
  EXPECT_FALSE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_FALSE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Open devtools for the active side of the split and verify it exists only
  // for the active side.
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  EXPECT_TRUE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_FALSE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Activate to the inactive side and verify it stayed open on the appropriate
  // side of the split.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_TRUE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Activate to the other split and verify no devtools are seen.
  browser()->tab_strip_model()->ActivateTabAt(2);
  EXPECT_FALSE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_FALSE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Switch back to the split where devtools is open and verify is is still
  // visible.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_TRUE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Verify two devtools can be seen at once (one for each side of a split).
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  EXPECT_TRUE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_TRUE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());
}

// Verifies that page and devtools WebViews are being correctly laid out
// when DevTools is opened/closed/updated while docked.
IN_PROC_BROWSER_TEST_F(BrowserViewTest,
                       DevToolsRemainsCorrectlyDockedAfterSwappingSplit) {
  // Add enough tabs to create two split views.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  // Add tabs to splits.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  browser()->tab_strip_model()->ActivateTabAt(0);

  // Open devtools for the active side of the split and verify it exists only
  // for the active side.
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  EXPECT_TRUE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_FALSE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());

  // Reverse the split and verify the correct side has devtools.
  browser_view()->multi_contents_view()->OnSwap();
  EXPECT_TRUE(
      active_contents_container_view()->devtools_web_view()->GetVisible());
  EXPECT_FALSE(
      inactive_contents_container_view()->devtools_web_view()->GetVisible());
}

// TODO(crbug.com/425715421): Re-enable when wayland supports drag and drop
#if !BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_DragNotSupportedInFullscreen DragNotSupportedInFullscreen
#else
#define MAYBE_DragNotSupportedInFullscreen DISABLED_DragNotSupportedInFullscreen
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_DragNotSupportedInFullscreen) {
  // Add enough tabs to create two split views.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  // Add tabs to splits.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Make fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Attempt to start a drag
  content::DropData drop_data;
  drop_data.url_infos = {
      ui::ClipboardUrlInfo(GURL("https://mail.google.com"), u"")};
  const gfx::Rect bounds = browser_view()->GetBoundsInScreen();
  const gfx::PointF point(bounds.left_center().x() + 10,
                          bounds.left_center().y());
  browser_view()->PreHandleDragUpdate(drop_data, point);

  EXPECT_FALSE(browser_view()
                   ->multi_contents_view()
                   ->drop_target_controller()
                   .IsDropTimerRunningForTesting());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, ScrimForTabModalInSplitView) {
  // Create a split view with two tabs followed by a third that will show the
  // scrim.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Show a tab modal dialog on the third tab (not part of the split).
  browser()->tab_strip_model()->ActivateTabAt(2);
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  auto delegate = std::make_unique<TestTabModalConfirmDialogDelegate>(contents);
  TabModalConfirmDialog::Create(std::move(delegate), contents);
  EXPECT_TRUE(
      active_contents_container_view()->contents_scrim_view()->GetVisible());

  // Activating a tab in the split will cause the scrim to hide.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(
      active_contents_container_view()->contents_scrim_view()->GetVisible());

  // Swapping the tab with the tab modal dialog into the inactive spot in the
  // split should show the scrim but not change the active tab.
  browser()->tab_strip_model()->UpdateTabInSplit(
      browser()->tab_strip_model()->GetTabAtIndex(1), 2,
      TabStripModel::SplitUpdateType::kSwap);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_TRUE(
      inactive_contents_container_view()->contents_scrim_view()->GetVisible());
  EXPECT_FALSE(
      active_contents_container_view()->contents_scrim_view()->GetVisible());
}

// Tests that GetAccessibleTabLabel correctly labels each tab in a split.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, AccessibleTabLabel) {
  // Create a pinned split.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->SetTabPinned(0, true);
  browser()->tab_strip_model()->SetTabPinned(1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_TAB_AX_LABEL_PINNED_FORMAT,
                l10n_util::GetStringFUTF16(
                    IDS_TAB_AX_LABEL_SPLIT_TAB_LEFT_VIEW_FORMAT,
                    browser()->GetTitleForTab(0))),
            browser_view()->GetAccessibleTabLabel(0));
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_TAB_AX_LABEL_PINNED_FORMAT,
                l10n_util::GetStringFUTF16(
                    IDS_TAB_AX_LABEL_SPLIT_TAB_RIGHT_VIEW_FORMAT,
                    browser()->GetTitleForTab(1))),
            browser_view()->GetAccessibleTabLabel(1));

  // Create a split.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(2);
  browser()->tab_strip_model()->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_SPLIT_TAB_LEFT_VIEW_FORMAT,
                                 browser()->GetTitleForTab(2)),
      browser_view()->GetAccessibleTabLabel(2));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_SPLIT_TAB_RIGHT_VIEW_FORMAT,
                                 browser()->GetTitleForTab(3)),
      browser_view()->GetAccessibleTabLabel(3));

  // Create a grouped split.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(4);
  browser()->tab_strip_model()->AddToNewSplit(
      {5}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  browser()->tab_strip_model()->AddToNewGroup({4, 5});
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                l10n_util::GetStringFUTF16(
                    IDS_TAB_AX_LABEL_SPLIT_TAB_LEFT_VIEW_FORMAT,
                    browser()->GetTitleForTab(4))),
            browser_view()->GetAccessibleTabLabel(4));
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                l10n_util::GetStringFUTF16(
                    IDS_TAB_AX_LABEL_SPLIT_TAB_RIGHT_VIEW_FORMAT,
                    browser()->GetTitleForTab(5))),
            browser_view()->GetAccessibleTabLabel(5));
}

#if BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(BrowserViewTest, SplitViewFullscreenLayout) {
  // Disable always show toolbar in fullscreen
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);

  // Create tabs and add to split
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_TRUE(browser()->tab_strip_model()->selection_model().IsSelected(0));
  ASSERT_TRUE(browser()->tab_strip_model()->selection_model().IsSelected(1));

  TopContainerView* top_container = browser_view()->top_container();
  views::View* overlay_view = browser_view()->overlay_view();

  // Verify top_container is parented to browser_view before fullscreen
  EXPECT_EQ(browser_view(), top_container->parent());
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Verify top_container is parented to overlay after entering fullscreen
  EXPECT_EQ(overlay_view, top_container->parent());

  browser_view()->GetExclusiveAccessContext()->ExitFullscreen();

  // Verify top_container is re-parented to browser_view after fullscreen exit
  EXPECT_EQ(browser_view(), top_container->parent());
}

IN_PROC_BROWSER_TEST_F(BrowserViewTest, SplitViewTabRevealFullscreen) {
  // Disable always show toolbar in fullscreen
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);

  // Create tabs and add to split
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_TRUE(browser()->tab_strip_model()->selection_model().IsSelected(0));
  ASSERT_TRUE(browser()->tab_strip_model()->selection_model().IsSelected(1));

  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_FALSE(browser()->window()->IsToolbarShowing());

  // Switching between split tabs does not reveal top container.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_FALSE(browser()->window()->IsToolbarShowing());

  // Switching to tab not in split should reveal top container.
  browser()->tab_strip_model()->ActivateTabAt(2);
  ASSERT_TRUE(browser()->window()->IsToolbarShowing());
}
#endif

namespace {

class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    auto response = std::make_unique<safe_browsing::RTLookupResponse>();

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(response_callback),
                       /*is_rt_lookup_successful=*/true,
                       /*is_cached_response=*/true, std::move(response)));
  }
};

class BrowserViewDataProtectionTest : public InProcessBrowserTest {
 public:
  BrowserViewDataProtectionTest()
      : scoped_prewarm_feature_list_(test::ScopedPrewarmFeatureList::
                                         PrewarmState::kEnabledWithNoTrigger) {
    scoped_feature_list_.InitAndEnableFeature(features::kSideBySide);
  }
  BrowserViewDataProtectionTest(const BrowserViewDataProtectionTest&) = delete;
  BrowserViewDataProtectionTest& operator=(
      const BrowserViewDataProtectionTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a DM token since the enterprise real-time URL service expects one.
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    auto create_service_callback =
        base::BindRepeating([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);

          // Enable real-time URL checks.
          profile->GetPrefs()->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
              enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
          profile->GetPrefs()->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
              policy::POLICY_SCOPE_MACHINE);

          auto testing_factory =
              base::BindRepeating([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<FakeRealTimeUrlLookupService>();
              });
          safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
              GetInstance()
                  ->SetTestingFactory(context, testing_factory);
        });

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(create_service_callback);
  }

  content::WebContents* NavigateAsync(const GURL& url) {
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    Navigate(&params);
    return params.navigated_or_inserted_contents;
  }

  void NavigateToAndWait(const GURL& url) {
    content::WaitForLoadStop(NavigateAsync(url));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  // TODO(https://crbug.com/458274323): browser()->GetWidget() seems returning
  // a wrong Widget, one for the prewarm page, unexpectedly, might be due to
  // missing MPArch support?
  // Investigate details, and fix it to remove this workaround so that
  // DC_Screenshot test can pass stably.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(BrowserViewDataProtectionTest, DC_Screenshot) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["noscreenshot.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  auto* widget = BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  ASSERT_TRUE(widget);

  NavigateToAndWait(GURL("https://noscreenshot.com"));
  EXPECT_FALSE(widget->AreScreenshotsAllowed());

  NavigateToAndWait(GURL("https://screenshot.com"));
  EXPECT_TRUE(widget->AreScreenshotsAllowed());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserViewChromeOSTestNoWebUiTabStrip,
                       EnsureViewTreeOrder) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const immersive_mode_controller =
      ImmersiveModeController::From(browser());

  std::vector<views::View*> children_before;
  for (const auto& child : browser_view->children()) {
    children_before.push_back(child);
  }

  EnterTabletMode();

  std::vector<views::View*> children_in_tablet;
  for (const auto& child : browser_view->children()) {
    children_in_tablet.push_back(child);
  }

  // Enter immersive fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // Exit immersive fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  std::vector<views::View*> children_in_tablet_after_immersive;
  for (const auto& child : browser_view->children()) {
    children_in_tablet_after_immersive.push_back(child);
  }

  // View tree order before and after immersive mode should be the same in
  // tablet mode.
  EXPECT_EQ(children_in_tablet, children_in_tablet_after_immersive);

  ExitTabletMode();

  std::vector<views::View*> children_after;
  for (const auto& child : browser_view->children()) {
    children_after.push_back(child);
  }

  // View tree order should be unchanged before and after tablet mode.
  EXPECT_EQ(children_before, children_after);
}

IN_PROC_BROWSER_TEST_F(BrowserViewChromeOSTestNoWebUiTabStrip,
                       TabStripParentedToTopContainer) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_EQ(browser_view->tab_strip_view()->parent(), browser_view);

  EnterTabletMode();
  EXPECT_EQ(browser_view->tab_strip_view()->parent(),
            static_cast<views::View*>(browser_view->top_container()));

  ExitTabletMode();
  EXPECT_EQ(browser_view->tab_strip_view()->parent(), browser_view);
}
#endif  // BUILDFLAG(CHROME_OS)

namespace {

// chrome/test/data/simple.html
const char kSimplePage[] = "/simple.html";

class BrowserViewScrimPixelTest : public UiBrowserTest {
 public:
  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL(kSimplePage);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    browser()->window()->Show();
    BrowserView::GetBrowserViewForBrowser(browser())
        ->GetActiveContentsContainerView()
        ->contents_scrim_view()
        ->SetVisible(true);
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose();
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserViewScrimPixelTest, InvokeUi_content_scrim) {
  ShowAndVerifyUi();
}
