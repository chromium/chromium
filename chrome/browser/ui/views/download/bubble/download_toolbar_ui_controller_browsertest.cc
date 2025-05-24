// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"

#include "base/files/file_path.h"
#include "base/test/run_until.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

// This tests the download button view in the toolbar.
// TODO(chlily): Add more tests to cover all functionality.
class DownloadToolbarUIControllerBrowserTest : public DownloadTestBase {
 public:
  DownloadToolbarUIControllerBrowserTest() = default;

  DownloadToolbarUIController* controller(Browser* browser) {
    return browser->GetFeatures().download_toolbar_ui_controller();
  }

  void SetUp() override {
    DownloadTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    // OS integration is needed to be able to launch web applications. This
    // override ensures OS integration doesn't leave any traces.
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
    DownloadTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      web_app::test::UninstallAllWebApps(profile);
    }
    override_registration_.reset();
    DownloadTestBase::TearDownOnMainThread();
  }

  PinnedToolbarActionsContainer* toolbar_container(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar_button_provider()
        ->GetPinnedToolbarActionsContainer();
  }

  ToolbarButton* toolbar_button(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar_button_provider()
        ->GetDownloadButton();
  }

  WebAppFrameToolbarTestHelper& toolbar_helper() {
    return web_app_frame_toolbar_helper_;
  }

 protected:
  void ClickButton(views::Button* button) {
    button->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  // OS integration is needed to be able to launch web applications. This
  // override ensures OS integration doesn't leave any traces.
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

// DownloadToolbarUIController and downloads toolbar button do not exist for
// ChromeOS. See https://crbug.com/1323505.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest, ShowHide) {
  EXPECT_EQ(toolbar_button(browser()), nullptr);
  controller(browser())->Show();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  controller(browser())->Hide();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_EQ(toolbar_button(browser()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       HideDoesNotRemoveButtonIfPinned) {
  EXPECT_EQ(toolbar_button(browser()), nullptr);
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  // Verify calling Hide does not change the visibility of the button when
  // pinned.
  controller(browser())->Hide();
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ControllerUpdatesButtonEnabledState) {
  // Pin downloads to the toolbar.
  EXPECT_EQ(toolbar_button(browser()), nullptr);
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  EXPECT_TRUE(toolbar_button(browser())->GetEnabled());
  // Disable the via the controller and verify the button's enabled state.
  controller(browser())->Disable();
  EXPECT_FALSE(toolbar_button(browser())->GetEnabled());
  // Enable the via the controller and verify the button's enabled state.
  controller(browser())->Enable();
  EXPECT_TRUE(toolbar_button(browser())->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonShowsForDownloadingItems) {
  EXPECT_EQ(toolbar_button(browser()), nullptr);
  // Download a file and verify the download button appears after the download.
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  // Verify calling Hide does change the visibility of the button.
  controller(browser())->Hide();
  EXPECT_EQ(toolbar_button(browser()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonShowsInNewBrowserWithRecentDownload) {
  EXPECT_EQ(toolbar_button(browser()), nullptr);
  // Download a file and verify the download button appears after the download.
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  // Create another browser and set it as active so the button becomes dormant.
  Browser* extra_browser = CreateBrowser(browser()->profile());
  BrowserList::SetLastActive(extra_browser);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(extra_browser), nullptr);
  EXPECT_TRUE(toolbar_button(extra_browser)->GetVisible());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonPressWithNoRecentDownloads) {
  // Pin the downloads button so it is available to press.
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowDownloads, true);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  {
    content::LoadStopObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ClickButton(toolbar_button(browser()));
    observer.Wait();
  }
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ButtonPressWithRecentDownloads) {
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  ClickButton(toolbar_button(browser()));
  EXPECT_EQ(controller(browser())->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       DownloadsAppearsinWebAppWithRecentDownload) {
  const GURL app_url("https://example.com/");
  // Create a web app, download from the web app, and verify the downloads
  // button appears.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = GURL();

  webapps::AppId app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);
  ui_test_utils::DownloadURL(
      app_browser, ui_test_utils::GetTestUrl(
                       base::FilePath().AppendASCII("downloads"),
                       base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(app_browser));
  EXPECT_NE(toolbar_button(app_browser), nullptr);
  EXPECT_TRUE(toolbar_button(app_browser)->GetVisible());
  // Close web app.
  CloseBrowserSynchronously(app_browser);

  // Reopen web app and verify download button appears.
  Browser* app_browser2 =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(app_browser2));
  EXPECT_NE(toolbar_button(app_browser2), nullptr);
  EXPECT_TRUE(toolbar_button(app_browser2)->GetVisible());

  // Click the button and verify the bubble opens.
  ClickButton(toolbar_button(app_browser2));
  EXPECT_EQ(controller(app_browser2)
                ->bubble_contents_for_testing()
                ->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       DialogAutoCloses) {
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  controller(browser())->ShowDetails();
  controller(browser())->OpenPrimaryDialog();
  EXPECT_EQ(controller(browser())->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
  controller(browser())
      ->auto_close_bubble_timer_for_testing()
      ->user_task()
      .Run();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return controller(browser())->bubble_contents_for_testing() == nullptr;
  }));
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       OpenPrimaryDialog) {
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  controller(browser())->ShowDetails();
  controller(browser())->OpenPrimaryDialog();
  EXPECT_EQ(controller(browser())->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       OpenSecurityDialog) {
  // Disable SafeBrowsing and make a download dangerous so that showing the
  // security view is valid.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);

  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), download_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  dangerous_observer->WaitForFinished();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1UL, download_items.size());
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));

  offline_items_collection::ContentId content_id =
      OfflineItemUtils::GetContentIdForDownload(download_items[0].get());
  controller(browser())->OpenSecuritySubpage(content_id);
  EXPECT_EQ(controller(browser())->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);
  EXPECT_EQ(controller(browser())
                ->bubble_contents_for_testing()
                ->security_view_for_testing()
                ->content_id(),
            content_id);
  controller(browser())
      ->bubble_contents_for_testing()
      ->ProcessSecuritySubpageButtonPress(content_id,
                                          DownloadCommands::Command::DISCARD);
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ProgressRingVisibleDuringDownload) {
  controller(browser())->Show();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  EXPECT_FALSE(
      controller(browser())->IsProgressRingInDownloadingStateForTesting());
  download::DownloadItem* download_item = CreateSlowTestDownload();
  EXPECT_TRUE(
      controller(browser())->IsProgressRingInDownloadingStateForTesting());
  download_item->Cancel(true);
  EXPECT_FALSE(
      controller(browser())->IsProgressRingInDownloadingStateForTesting());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ProgressRingDormantState) {
  EXPECT_FALSE(controller(browser())->IsProgressRingInDormantStateForTesting());
  download::DownloadItem* download_item = CreateSlowTestDownload();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  EXPECT_FALSE(controller(browser())->IsProgressRingInDormantStateForTesting());
  // Create another browser and set it as active so the button becomes dormant.
  Browser* extra_browser = CreateBrowser(browser()->profile());
  BrowserList::SetLastActive(extra_browser);
  views::test::WaitForAnimatingLayoutManager(toolbar_container(extra_browser));

  EXPECT_TRUE(controller(browser())->IsProgressRingInDormantStateForTesting());
  download_item->Cancel(true);
  EXPECT_FALSE(controller(browser())->IsProgressRingInDormantStateForTesting());
}

IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ImageBadgeDoesNotShowForSingleDownload) {
  download::DownloadItem* download_item = CreateSlowTestDownload();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());
  EXPECT_TRUE(controller(browser())
                  ->GetImageBadgeForTesting()
                  ->GetImageModel()
                  .IsEmpty());
  download_item->Cancel(true);
}

// https://crbug.com/402616838: Test is flaky, disabled for investigation.
IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       DISABLED_ImageBadgeShowsForMultipleDownloads) {
  controller(browser())->Show();
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  EXPECT_NE(toolbar_button(browser()), nullptr);
  EXPECT_TRUE(toolbar_button(browser())->GetVisible());

  content::DownloadManager* manager = DownloadManagerForBrowser(browser());

  std::unique_ptr<content::DownloadTestObserver> observer =
      std::make_unique<content::DownloadTestObserverInProgress>(manager, 2);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl);

  // Start two slow in progress downloads.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  observer->WaitForFinished();

  // Verify the image badge shows when there are two in progress downloads.
  EXPECT_EQ(2u, observer->NumDownloadsSeenInState(
                    download::DownloadItem::IN_PROGRESS));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto* image_badge = controller(browser())->GetImageBadgeForTesting();
    return image_badge && !image_badge->GetImageModel().IsEmpty();
  }));

  content::DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  for (download::DownloadItem* item : items) {
    item->Cancel(true);
  }
}

// Asserts that when a browser is closed with an open download bubble the
// browser does not crash.
IN_PROC_BROWSER_TEST_F(DownloadToolbarUIControllerBrowserTest,
                       ClosingBrowserWithOpenBubbleDoesNotCrash) {
  ui_test_utils::DownloadURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath().AppendASCII("downloads"),
                     base::FilePath().AppendASCII("a_zip_file.zip")));
  views::test::WaitForAnimatingLayoutManager(toolbar_container(browser()));
  controller(browser())->ShowDetails();
  controller(browser())->OpenPrimaryDialog();
  EXPECT_EQ(controller(browser())->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
  CloseBrowserSynchronously(browser());
}

#endif
