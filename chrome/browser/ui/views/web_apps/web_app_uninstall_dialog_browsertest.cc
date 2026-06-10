// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_identity_view.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/sub_app_identity_view.h"
#include "chrome/browser/ui/views/web_apps/sub_apps/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

using webapps::AppId;

namespace {

webapps::AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");

  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  web_app_info->scope = example_url;
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

}  // namespace

class WebAppUninstallDialogViewBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  web_app::WebAppProvider* provider() {
    return web_app::WebAppProvider::GetForTest(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionAfterViewCreation) {
  webapps::AppId app_id = InstallTestWebApp(browser()->profile());

  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  views::NamedWidgetShownWaiter uninstall_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  auto* uninstall_widget = uninstall_dialog_waiter.WaitIfNeededAndGet();
  EXPECT_NE(uninstall_widget, nullptr);

  // Kill parent window.
  browser()->GetWindow()->Close();
  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kCancelled);
}

// Uninstalling with no browser window open can cause the view to be destroyed
// before the views object. Test that this does not cause a UAF. See
// https://crbug.com/40053916.
// Also tests that we don't crash when uninstalling a web app from a web app
// window in Ash. See https://crbug.com/40568607.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionBeforeViewCreation) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  webapps::AppId app_id = InstallTestWebApp(browser()->profile());
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);
  EXPECT_NE(app_browser, browser());
  chrome::CloseWindow(browser());

  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      app_browser->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kAppRemoved);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Cancel) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);
  webapps::AppId app_id = InstallTestWebApp(browser()->profile());

  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kCancelled);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Accept) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION);
  webapps::AppId app_id = InstallTestWebApp(browser()->profile());

  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kAppRemoved);
}

class WebAppUninstallDialogViewIwaBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
 protected:
  web_app::WebAppProvider* provider() {
    return web_app::WebAppProvider::GetForTest(profile());
  }

  webapps::AppId InstallSubAppAndWait(content::WebContents* iwa_contents,
                                      std::string_view install_url,
                                      const GURL& expected_sub_app_url) {
    auto dialog_override =
        web_app::SubAppsInstallDialogController::SetAutomaticActionForTesting(
            web_app::SubAppsInstallDialogController::DialogActionForTesting::
                kAccept);

    base::test::TestFuture<webapps::AppId> test_future;
    web_app::WebAppInstallManagerObserverAdapter observer(profile());
    observer.SetWebAppInstalledWithOsHooksDelegate(
        test_future.GetRepeatingCallback<const webapps::AppId&>());

    EXPECT_TRUE(content::ExecJs(iwa_contents,
                                base::ReplaceStringPlaceholders(
                                    R"(
              navigator.subApps.add({
                "$1": {"installURL": "$1"}
              })
            )",
                                    {std::string(install_url)}, nullptr)));

    webapps::AppId sub_app_id = web_app::GenerateAppId(
        /*manifest_id_path=*/std::nullopt, expected_sub_app_url);

    EXPECT_EQ(sub_app_id, test_future.Take());
    return sub_app_id;
  }

 private:
  base::test::ScopedFeatureList features_{blink::features::kSubApps};
};

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewIwaBrowserTest,
                       UninstallIwaShowsIsolatedWebAppIdentityView) {
  auto bundle =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().SetName("Test IWA").SetVersion("1.2.3"))
          .BuildBundle();
  web_app::IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
  webapps::AppId app_id = url_info.app_id();

  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  views::NamedWidgetShownWaiter uninstall_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  auto* uninstall_widget = uninstall_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(uninstall_widget, nullptr);

  views::ElementTrackerViews* tracker_views =
      views::ElementTrackerViews::GetInstance();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(uninstall_widget);

  EXPECT_NE(
      tracker_views->GetUniqueView(
          IsolatedWebAppIdentityView::kIsolatedWebAppIdentityViewId, context),
      nullptr);
  EXPECT_EQ(
      tracker_views->GetUniqueView(
          WebAppUninstallDialogDelegateView::kUninstallCheckboxId, context),
      nullptr);

  views::View* title_view = tracker_views->GetUniqueView(
      web_app::kSimpleInstallDialogAppTitle, context);
  ASSERT_NE(title_view, nullptr);
  EXPECT_THAT(base::UTF16ToUTF8(
                  views::AsViewClass<views::Label>(title_view)->GetText()),
              testing::HasSubstr("Test IWA"));

  views::View* info_view = tracker_views->GetUniqueView(
      web_app::kSimpleInstallDialogAppInfoLabel, context);
  ASSERT_NE(info_view, nullptr);
  EXPECT_THAT(
      base::UTF16ToUTF8(views::AsViewClass<views::Label>(info_view)->GetText()),
      testing::HasSubstr("1.2.3"));

  uninstall_widget->CloseNow();
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewIwaBrowserTest,
                       UninstallSubAppShowsSubAppIdentityView) {
  // Install parent IWA that supports sub-apps.
  auto parent_bundle =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .SetName("Parent IWA")
              .AddPermissionsPolicy(
                  network::mojom::PermissionsPolicyFeature::kSubApps,
                  /*self=*/true, /*origins=*/{}))
          .AddHtml("/", "<h1>Parent</h1>")
          .AddHtml("/subapp/index.html", R"(
            <!DOCTYPE html>
            <html>
              <head>
                <link rel="manifest" href="/subapp.webmanifest">
                <title>Sub App</title>
              </head>
              <body><h1>Sub App</h1></body>
            </html>
          )")
          .AddResource("/subapp.webmanifest",
                       R"(
            {
              "name": "Sub App",
              "version": "1.0.0",
              "start_url": "/subapp/index.html",
              "display": "standalone",
              "icons": [{
                "src": "/icon.png",
                "sizes": "256x256",
                "type": "image/png"
              }]
            }
          )",
                       "application/manifest+json")
          .BuildBundle();
  web_app::IsolatedWebAppUrlInfo parent_url_info =
      parent_bundle->InstallChecked(profile());

  // Launch parent IWA.
  Browser* parent_browser =
      LaunchWebAppBrowserAndWait(parent_url_info.app_id());
  ASSERT_NE(parent_browser, nullptr);
  content::WebContents* parent_contents =
      parent_browser->tab_strip_model()->GetActiveWebContents();

  // Install sub-app.
  webapps::AppId sub_app_id = InstallSubAppAndWait(
      parent_contents, "/subapp/index.html",
      parent_url_info.origin().GetURL().Resolve("/subapp/index.html"));

  // Trigger uninstall for sub-app.
  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  provider()->ui_manager().PresentUserUninstallDialog(
      sub_app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->GetWindow()->GetNativeWindow(), test_future.GetCallback());

  // Wait for dialog.
  views::NamedWidgetShownWaiter uninstall_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  auto* uninstall_widget = uninstall_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(uninstall_widget, nullptr);

  views::ElementTrackerViews* tracker_views =
      views::ElementTrackerViews::GetInstance();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(uninstall_widget);

  // Verify SubAppIdentityView is present.
  EXPECT_NE(tracker_views->GetUniqueView(
                SubAppIdentityView::kSubAppIdentityViewId, context),
            nullptr);
  EXPECT_EQ(
      tracker_views->GetUniqueView(
          WebAppUninstallDialogDelegateView::kUninstallCheckboxId, context),
      nullptr);

  views::View* title_view = tracker_views->GetUniqueView(
      web_app::kSimpleInstallDialogAppTitle, context);
  ASSERT_NE(title_view, nullptr);
  EXPECT_THAT(base::UTF16ToUTF8(
                  views::AsViewClass<views::Label>(title_view)->GetText()),
              testing::HasSubstr("Sub App"));

  views::View* info_view = tracker_views->GetUniqueView(
      web_app::kSimpleInstallDialogAppInfoLabel, context);
  ASSERT_NE(info_view, nullptr);
  EXPECT_THAT(
      base::UTF16ToUTF8(views::AsViewClass<views::Label>(info_view)->GetText()),
      testing::HasSubstr("Parent IWA"));

  uninstall_widget->CloseNow();
}
