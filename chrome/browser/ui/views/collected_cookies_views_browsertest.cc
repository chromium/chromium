// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/test/widget_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
const char kCookiesDialogHistogramName[] = "Privacy.CookiesInUseDialog.Action";
constexpr std::array<const char*, 7> kSiteDataTypes{
    "Cookie", "LocalStorage",  "SessionStorage", "IndexedDb",
    "WebSql", "ServiceWorker", "CacheStorage"};

}  // namespace

class CollectedCookiesViewsTest : public InProcessBrowserTest {
 public:
  CollectedCookiesViewsTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched. Disable features for the new version of "Cookies in use"
    // dialog. These tests are for the current version of the dialog only.
    feature_list_.InitWithFeatures({}, {page_info::kPageSpecificSiteDataDialog,
                                        page_info::kPageInfoCookiesSubpage});
  }

  CollectedCookiesViewsTest(const CollectedCookiesViewsTest&) = delete;
  CollectedCookiesViewsTest& operator=(const CollectedCookiesViewsTest&) =
      delete;

  ~CollectedCookiesViewsTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Disable cookies.
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

    // Load a page with cookies.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/cookie1.html")));

    // Spawn a cookies dialog.
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
        web_contents);
    cookies_dialog_ =
        PageSpecificSiteDataDialogController::GetDialogViewForTesting(
            web_contents);
  }

  // Closing dialog with modified data will shows infobar.
  void SetDialogChanged() { cookies_dialog_->set_status_changed_for_testing(); }

  void CloseCookiesDialog() { cookies_dialog_->GetWidget()->Close(); }

  size_t infobar_count() const {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents
               ? infobars::ContentInfoBarManager::FromWebContents(web_contents)
                     ->infobar_count()
               : 0;
  }

 private:
  raw_ptr<CollectedCookiesViews, DanglingUntriaged> cookies_dialog_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsTest, CloseDialog) {
  // Test closing dialog without changing data.
  CloseCookiesDialog();
  EXPECT_EQ(0u, infobar_count());
}

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsTest, ChangeAndCloseDialog) {
  // Test closing dialog with changing data. Dialog will show infobar.
  SetDialogChanged();
  CloseCookiesDialog();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsTest, ChangeAndNavigateAway) {
  // Test navigation after changing dialog data. Changed dialog should not show
  // infobar or crash because infobars::ContentInfoBarManager is gone.

  SetDialogChanged();

  // Navigation in the owning tab will close dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie2.html")));

  EXPECT_EQ(0u, infobar_count());
}

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsTest, ChangeAndCloseTab) {
  // Test closing tab after changing dialog data. Changed dialog should not
  // show infobar or crash because infobars::ContentInfoBarManager is gone.

  SetDialogChanged();

  // Closing the owning tab will close dialog.
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  EXPECT_EQ(0u, infobar_count());
}

// Closing the widget asynchronously destroys the CollectedCookiesViews object,
// but synchronously removes it from the WebContentsModalDialogManager. Make
// sure there's no crash when trying to re-open the CollectedCookiesViews right
// after closing it. Regression test for https://crbug.com/989888
IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsTest, CloseDialogAndReopen) {
  CloseCookiesDialog();
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
      web_contents);
  // If the test didn't crash, it has passed.
}

class CollectedCookiesViewsMetricsTest : public InProcessBrowserTest {
 public:
  CollectedCookiesViewsMetricsTest() {
    // Disable features for the new version of "Cookies in use" dialog. These
    // tests are for the current version of the dialog only.
    feature_list_.InitWithFeatures({}, {page_info::kPageSpecificSiteDataDialog,
                                        page_info::kPageInfoCookiesSubpage});
  }

  CollectedCookiesViewsMetricsTest(const CollectedCookiesViewsMetricsTest&) =
      delete;
  CollectedCookiesViewsMetricsTest& operator=(
      const CollectedCookiesViewsMetricsTest&) = delete;

  ~CollectedCookiesViewsMetricsTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/browsing_data/site_data.html")));

    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    SetupSiteData(web_contents);
  }

  void ClickButton(ui::ElementIdentifier button_id) {
    auto* button =
        static_cast<views::Button*>(GetViewByElementIdentifier(button_id));
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  views::View* GetViewByElementIdentifier(ui::ElementIdentifier id) {
    auto* element_tracker = ui::ElementTracker::GetElementTracker();
    auto* tracked_element = element_tracker->GetFirstMatchingElement(
        id, browser()->window()->GetElementContext());
    auto* view = tracked_element->AsA<views::TrackedElementViews>()->view();
    return view;
  }
  void OpenCookiesInUseDialog() {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "CollectedCookiesViews");
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
        web_contents);
    waiter.WaitIfNeededAndGet();
  }
  void WaitForData() {
    auto* allowed_cookies_tree =
        static_cast<views::TreeView*>(GetViewByElementIdentifier(
            CollectedCookiesViews::kAllowedCookiesTreeElementId));
    auto* model = static_cast<CookiesTreeModel*>(allowed_cookies_tree->model());
    auto* site_node = model->GetChildren(model->GetRoot()).back();
    // Wait until data is loaded.
    while (model->GetChildren(site_node).size() != 7) {
      base::RunLoop().RunUntilIdle();
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  void SetupSiteData(content::WebContents* web_contents) {
    for (const char* data_type : kSiteDataTypes) {
      SetDataForType(data_type, web_contents);
      EXPECT_TRUE(HasDataForType(data_type, web_contents));
    }
  }

  void SetDataForType(const std::string& type,
                      content::WebContents* web_contents) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()", web_contents))
        << "Couldn't create data for: " << type;
  }

  bool HasDataForType(const std::string& type,
                      content::WebContents* web_contents) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return RunScriptAndGetBool("has" + type + "()", web_contents);
  }

  bool RunScriptAndGetBool(const std::string& script,
                           content::WebContents* web_contents) {
    EXPECT_TRUE(web_contents);
    return content::EvalJs(web_contents, script).ExtractBool();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsMetricsTest, OpenDialog) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;
  const std::string open_action = "CookiesInUseDialog.Opened";

  // The histogram should start empty and no actions recorded.
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);
  EXPECT_EQ(0, user_actions.GetActionCount(open_action));

  // Open Cookies in use dialog.
  OpenCookiesInUseDialog();

  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 1);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kDialogOpened), 1);
  EXPECT_EQ(1, user_actions.GetActionCount(open_action));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_DeleteFolder DISABLED_DeleteFolder
#else
#define MAYBE_DeleteFolder DeleteFolder
#endif
IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsMetricsTest, MAYBE_DeleteFolder) {
  base::HistogramTester histograms;
  // The histogram should start empty.
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  // Open Cookies in use dialog.
  OpenCookiesInUseDialog();

  // Wait until data is loaded.
  WaitForData();

  auto* allowed_cookies_tree =
      static_cast<views::TreeView*>(GetViewByElementIdentifier(
          CollectedCookiesViews::kAllowedCookiesTreeElementId));

  auto* model = static_cast<CookiesTreeModel*>(allowed_cookies_tree->model());
  allowed_cookies_tree->Expand(model->GetRoot());
  // Select the last folder in the list.
  auto* site_node = model->GetChildren(model->GetRoot()).back();
  allowed_cookies_tree->SetSelectedNode(model->GetChildren(site_node).back());
  // Delete folder.
  ClickButton(CollectedCookiesViews::kRemoveButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kFolderDeleted), 1);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_RemoveButton DISABLED_RemoveButton
#else
#define MAYBE_RemoveButton RemoveButton
#endif
IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsMetricsTest, MAYBE_RemoveButton) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;
  const std::string remove_action = "CookiesInUseDialog.RemoveButtonClicked";

  // The histogram should start empty and no actions recorded.
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);
  EXPECT_EQ(0, user_actions.GetActionCount(remove_action));

  // Opening Cookies in use dialog.
  OpenCookiesInUseDialog();

  // Wait until data is loaded.
  WaitForData();

  // Click remove on site level which is automatically selected upon dialog
  // spawn.
  ClickButton(CollectedCookiesViews::kRemoveButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kSiteDeleted), 1);
  EXPECT_EQ(1, user_actions.GetActionCount(remove_action));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_BlockAllowSite DISABLED_BlockAllowSite
#else
#define MAYBE_BlockAllowSite BlockAllowSite
#endif
IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsMetricsTest, MAYBE_BlockAllowSite) {
  base::HistogramTester histograms;

  // The histogram should start empty.
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  // Opening Cookies in use dialog.
  OpenCookiesInUseDialog();

  // Wait until data is loaded.
  WaitForData();

  // Block site.
  ClickButton(CollectedCookiesViews::kBlockButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kSiteBlocked), 1);
  // Close Cookies in use Dialog.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PageSpecificSiteDataDialogController::GetDialogViewForTesting(web_contents)
      ->GetWidget()
      ->Close();
  // Reload site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/browsing_data/site_data.html")));
  // Re-open Cookies in use dialog.
  OpenCookiesInUseDialog();

  // Select blocked tab.
  auto* tabbed_pane = static_cast<views::TabbedPane*>(
      GetViewByElementIdentifier(CollectedCookiesViews::kTabbedPaneElementId));
  tabbed_pane->SelectTabAt(1);
  // Allow site.
  ClickButton(CollectedCookiesViews::kAllowButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kSiteAllowed), 1);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_BlockClearOnExit DISABLED_BlockClearOnExit
#else
#define MAYBE_BlockClearOnExit BlockClearOnExit
#endif
IN_PROC_BROWSER_TEST_F(CollectedCookiesViewsMetricsTest,
                       MAYBE_BlockClearOnExit) {
  base::HistogramTester histograms;

  // The histogram should start empty.
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);
  // Opening Cookies in use dialog.
  OpenCookiesInUseDialog();
  // Wait until data is loaded.
  WaitForData();
  // Block site.
  ClickButton(CollectedCookiesViews::kBlockButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kSiteBlocked), 1);
  // Close Cookies in use Dialog.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PageSpecificSiteDataDialogController::GetDialogViewForTesting(web_contents)
      ->GetWidget()
      ->Close();
  // Reload site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/browsing_data/site_data.html")));
  // Re-open Cookies in use dialog.
  OpenCookiesInUseDialog();
  // Select blocked tab.
  auto* tabbed_pane = static_cast<views::TabbedPane*>(
      GetViewByElementIdentifier(CollectedCookiesViews::kTabbedPaneElementId));
  tabbed_pane->SelectTabAt(1);
  // Clear site on exit.
  ClickButton(CollectedCookiesViews::kClearOnExitButtonId);
  histograms.ExpectBucketCount(
      kCookiesDialogHistogramName,
      static_cast<int>(PageSpecificSiteDataDialogAction::kSiteClearedOnExit),
      1);
}
