// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/web_app_startup_utils.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/file_handler_launch_dialog_view.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {

namespace {

const char kStartUrl[] = "https://example.org/";
const char kFileLaunchUrl[] = "https://example.org/file_launch/";
const char kFileLaunchUrl2[] = "https://example.org/file_launch2/";

}  // namespace

// Tests for the `FileHandlerLaunchDialogView` as well as
// `startup::web_app::MaybeHandleWebAppLaunch()`. As Chrome OS uses the app
// service to launch PWAs, this test suite is not run there.
class FileHandlerLaunchDialogTest : public WebAppBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    test::WaitUntilReady(provider());
    InstallTestWebApp();
  }

  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->profile());
  }

  void LaunchAppWithFiles(const std::vector<base::FilePath>& paths) {
    StartupBrowserCreator browser_creator;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kAppId, app_id_);
    for (const auto& path : paths) {
      command_line.AppendArgPath(path);
    }

    browser_creator.Start(
        command_line, profile_manager->user_data_dir(),
        {browser()->profile(), StartupProfileMode::kBrowserWindow}, {});
  }

  void InstallTestWebApp() {
    const GURL example_url = GURL(kStartUrl);
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
    web_app_info->title = u"Test app";
    web_app_info->scope = example_url;
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;

    // Basic plain text format.
    apps::FileHandler entry1;
    entry1.action = GURL(kFileLaunchUrl);
    entry1.accept.emplace_back();
    entry1.accept[0].mime_type = "text/*";
    entry1.accept[0].file_extensions.insert(".txt");
    entry1.launch_type = apps::FileHandler::LaunchType::kSingleClient;
    web_app_info->file_handlers.push_back(std::move(entry1));

    // An image format.
    apps::FileHandler entry2;
    entry2.action = GURL(kFileLaunchUrl2);
    entry2.accept.emplace_back();
    entry2.accept[0].mime_type = "image/*";
    entry2.accept[0].file_extensions.insert(".png");
    entry2.launch_type = apps::FileHandler::LaunchType::kMultipleClients;
    web_app_info->file_handlers.push_back(std::move(entry2));

    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    provider()->scheduler().InstallFromInfoWithParams(
        std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());

    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      app_id_ = webapps::AppId();
      return;
    }

    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    app_id_ = result.Get<webapps::AppId>();

    // Setting the user display mode is necessary because
    // `test::InstallWebApp()` forces a kBrowser display mode; see
    // `WebAppInstallFinalizer::FinalizeInstall()`.
    ScopedRegistryUpdate update =
        provider()->sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(app_id_)->SetUserDisplayMode(
        mojom::UserDisplayMode::kStandalone);
  }

  const WebApp* GetApp() {
    return provider()->registrar_unsafe().GetAppById(app_id_);
  }

  // Launches the app and responds to the dialog, verifying expected outcomes.
  void LaunchAppAndRespond(bool remember_checkbox_state,
                           views::Widget::ClosedReason user_response,
                           ApiApprovalState expected_end_state,
                           std::vector<base::FilePath> file_paths = {},
                           GURL expected_url = {}) {
    content::TestNavigationObserver navigation_observer(expected_url);
    if (!expected_url.is_empty())
      navigation_observer.StartWatchingNewWebContents();

    base::RunLoop run_loop;
    web_app::startup::SetStartupDoneCallbackForTesting(run_loop.QuitClosure());

    FileHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(
        remember_checkbox_state);
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "FileHandlerLaunchDialogView");
    if (file_paths.empty())
      file_paths = {{base::FilePath::FromASCII("foo.txt")}};
    LaunchAppWithFiles(file_paths);
    waiter.WaitIfNeededAndGet()->CloseWithReason(user_response);
    run_loop.Run();
    EXPECT_EQ(expected_end_state, GetApp()->file_handler_approval_state());

    if (!expected_url.is_empty())
      navigation_observer.Wait();
  }

  // Launches the app to handle a file, assumes no dialog will be shown, but
  // waits for the app window to be launched to `expected_url`.
  void LaunchAppAndExpectUrlWithoutDialog(const base::FilePath& file,
                                          const GURL& expected_url) {
    content::TestNavigationObserver navigation_observer(expected_url);
    navigation_observer.StartWatchingNewWebContents();
    LaunchAppWithFiles({file});
    navigation_observer.Wait();
  }

  // Returns the URL of the first tab in the last opened browser.
  static GURL GetLastOpenedUrl() {
    auto* list = BrowserList::GetInstance();
    return list->get(list->size() - 1)
        ->tab_strip_model()
        ->GetWebContentsAt(0)
        ->GetLastCommittedURL();
  }

 protected:
  WebAppProvider* provider() {
    return WebAppProvider::GetForTest(browser()->profile());
  }

 private:
  webapps::AppId app_id_;
};

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest,
                       EscapeDoesNotRememberPreference) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  LaunchAppAndRespond(/*remember_checkbox_state=*/true,
                      views::Widget::ClosedReason::kEscKeyPressed,
                      ApiApprovalState::kRequiresPrompt);

  // One normal browser window exists still as the app wasn't launched.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, DisallowAndRemember) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app to handle files, deny at the prompt and "don't ask
  // again".
  LaunchAppAndRespond(/*remember_checkbox_state=*/true,
                      views::Widget::ClosedReason::kCancelButtonClicked,
                      ApiApprovalState::kDisallowed);
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app again. It should fail without showing a dialog. The
  // app window will be shown, but the files won't be passed.
  LaunchAppAndExpectUrlWithoutDialog(base::FilePath::FromASCII("foo.txt"),
                                     GURL(kStartUrl));
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());
  EXPECT_EQ(GURL(kStartUrl), GetLastOpenedUrl());
}

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, AllowAndRemember) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app to handle files, allow at the prompt and "don't ask
  // again".
  LaunchAppAndRespond(/*remember_checkbox_state=*/true,
                      views::Widget::ClosedReason::kAcceptButtonClicked,
                      ApiApprovalState::kAllowed,
                      /*file_paths=*/{}, GURL(kFileLaunchUrl));
  // An app window is created.
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());

  // Try to launch the app again. It should succeed without showing a dialog.
  LaunchAppAndExpectUrlWithoutDialog(base::FilePath::FromASCII("foo.txt"),
                                     GURL(kFileLaunchUrl));
  EXPECT_EQ(3U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(2)->is_type_app());
  EXPECT_EQ(GURL(kFileLaunchUrl), GetLastOpenedUrl());
}

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, DisallowDoNotRemember) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app to handle files, deny at the prompt and uncheck
  // "don't ask again".
  LaunchAppAndRespond(/*remember_checkbox_state=*/false,
                      views::Widget::ClosedReason::kCancelButtonClicked,
                      ApiApprovalState::kRequiresPrompt);
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app again. It should show a dialog again. This time,
  // accept.
  LaunchAppAndRespond(/*remember_checkbox_state=*/false,
                      views::Widget::ClosedReason::kAcceptButtonClicked,
                      ApiApprovalState::kRequiresPrompt,
                      /*file_paths=*/{}, GURL(kFileLaunchUrl));
  // An app window is created.
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());
  EXPECT_EQ(GURL(kFileLaunchUrl), GetLastOpenedUrl());
}

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, AcceptDoNotRemember) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app to handle files, allow at the prompt and uncheck
  // "don't ask again".
  LaunchAppAndRespond(/*remember_checkbox_state=*/false,
                      views::Widget::ClosedReason::kAcceptButtonClicked,
                      ApiApprovalState::kRequiresPrompt, /*file_paths=*/{},
                      GURL(kFileLaunchUrl));
  // An app window is created.
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());

  // Try to launch the app again. It should show a dialog again.
  LaunchAppAndRespond(/*remember_checkbox_state=*/false,
                      views::Widget::ClosedReason::kCancelButtonClicked,
                      ApiApprovalState::kRequiresPrompt);

  // An app window is not created.
  ASSERT_EQ(2U, BrowserList::GetInstance()->size());
}

// Regression test for crbug.com/1205528
IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, UnhandledType) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app with a file type it doesn't handle. It should fail
  // without showing a dialog, but fall back to showing a normal browser
  // window.
  LaunchAppAndExpectUrlWithoutDialog(base::FilePath::FromASCII("foo.rtf"),
                                     GURL(kStartUrl));
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());
  EXPECT_EQ(GURL(kStartUrl), GetLastOpenedUrl());
}

IN_PROC_BROWSER_TEST_F(FileHandlerLaunchDialogTest, MultiLaunch) {
  // One normal browser window exists.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Try to launch the app with two file types it handles, each one
  // corresponding to a different file handler. Only one launch dialog for two
  // windows that are opened.
  content::TestNavigationObserver navigation_observer1((GURL(kFileLaunchUrl)));
  content::TestNavigationObserver navigation_observer2((GURL(kFileLaunchUrl2)));
  navigation_observer1.StartWatchingNewWebContents();
  navigation_observer2.StartWatchingNewWebContents();
  LaunchAppAndRespond(false, views::Widget::ClosedReason::kAcceptButtonClicked,
                      ApiApprovalState::kRequiresPrompt,
                      {base::FilePath::FromASCII("foo.txt"),
                       base::FilePath::FromASCII("foo2.txt"),
                       base::FilePath::FromASCII("foo.png"),
                       base::FilePath::FromASCII("foo2.png")});
  navigation_observer1.Wait();
  navigation_observer2.Wait();

  // The two .png files should be directed to 2 different windows.
  ASSERT_EQ(4U, BrowserList::GetInstance()->size());
  EXPECT_TRUE(BrowserList::GetInstance()->get(1)->is_type_app());
  EXPECT_TRUE(BrowserList::GetInstance()->get(2)->is_type_app());
  EXPECT_TRUE(BrowserList::GetInstance()->get(3)->is_type_app());
}

}  // namespace web_app
