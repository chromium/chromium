// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_launch_dialog_view.h"
#include "chrome/browser/ui/views/web_apps/sub_apps/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr const char kProtocolHandlerIconUrl[] =
    "https://www.exmaple.org/protocol/icon";

webapps::AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");
  auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  app_info->title = u"Test app";
  app_info->scope = example_url;
  apps::ProtocolHandlerInfo protocol_handler;
  protocol_handler.protocol = "web+test";
  protocol_handler.url = GURL("http://example.org/?uri=%s");
  app_info->protocol_handlers.push_back(std::move(protocol_handler));

  // Add both manifest and trusted icons to `app_info` to test masking.
  const GeneratedIconsInfo any_icon_info1(IconPurpose::ANY, {icon_size::k32},
                                          {SK_ColorBLACK});
  const GeneratedIconsInfo any_icon_info2(IconPurpose::MASKABLE,
                                          {icon_size::k32}, {SK_ColorBLUE});
  web_app::AddIconsToWebAppInstallInfo(app_info.get(),
                                       GURL(kProtocolHandlerIconUrl),
                                       {any_icon_info1, any_icon_info2});

  return test::InstallWebApp(profile, std::move(app_info));
}

class ProtocolHandlerLaunchDialogBrowserTest : public WebAppBrowserTestBase {
 protected:
  void ShowDialogAndCloseWithReason(views::Widget::ClosedReason reason,
                                    bool expected_allowed,
                                    bool expected_remember_user_choice) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");
    GURL protocol_url("web+test://test");
    webapps::AppId test_app_id = InstallTestWebApp(browser()->profile());

    base::RunLoop run_loop;
    auto dialog_finished = base::BindLambdaForTesting(
        [&](bool allowed, bool remember_user_choice) {
          run_loop.Quit();
          EXPECT_EQ(expected_allowed, allowed);
          EXPECT_EQ(expected_remember_user_choice, remember_user_choice);
        });

    ShowWebAppProtocolLaunchDialog(protocol_url, browser()->profile(),
                                   test_app_id, std::move(dialog_finished));

    waiter.WaitIfNeededAndGet()->CloseWithReason(reason);
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(
    ProtocolHandlerLaunchDialogBrowserTest,
    WebAppProtocolHandlerIntentPickerDialog_EscapeDoesNotRememberPreference) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(views::Widget::ClosedReason::kEscKeyPressed,
                               /*expected_allowed=*/false,
                               /*expected_remember_user_choice=*/false);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_DisallowAndRemember) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked,
      /*expected_allowed=*/false,
      /*expected_remember_user_choice=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProtocolHandlerLaunchDialogBrowserTest,
    ProtocolHandlerIntentPickerDialog_DisallowDoNotRemember) {
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked,
      /*expected_allowed=*/false,
      /*expected_remember_user_choice=*/false);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_AcceptAndRemember) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked,
      /*expected_allowed=*/true,
      /*expected_remember_user_choice=*/true);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_AcceptDoNotRemember) {
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked,
      /*expected_allowed=*/true,
      /*expected_remember_user_choice=*/false);
}

class WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");
    GURL protocol_url("web+test://test");
    webapps::AppId test_app_id = InstallTestWebApp(browser()->profile());
    ShowWebAppProtocolLaunchDialog(protocol_url, browser()->profile(),
                                   test_app_id, base::DoNothing());
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kEscKeyPressed);
  }
};

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest,
    InvokeUi_CloseDialog) {
  ShowAndVerifyUi();
}

class ProtocolHandlerLaunchDialogIwaTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    test::WaitUntilReady(WebAppProvider::GetForTest(profile()));
  }

  webapps::AppId InstallSubAppAndWait(content::WebContents* iwa_contents,
                                      std::string_view install_url,
                                      const GURL& expected_sub_app_url) {
    auto dialog_override =
        SubAppsInstallDialogController::SetAutomaticActionForTesting(
            SubAppsInstallDialogController::DialogActionForTesting::kAccept);

    base::test::TestFuture<webapps::AppId> test_future;
    WebAppInstallManagerObserverAdapter observer(profile());
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

    webapps::AppId sub_app_id = GenerateAppId(
        /*manifest_id_path=*/std::nullopt, expected_sub_app_url);

    EXPECT_EQ(sub_app_id, test_future.Take());
    return sub_app_id;
  }

 private:
  base::test::ScopedFeatureList features_{blink::features::kSubApps};
};

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogIwaTest,
                       LaunchIwaShowsVersionLabel) {
  auto bundle =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .SetName("Test IWA")
              .SetVersion("3.4.5")
              .AddProtocolHandler("web+test", "/handle_protocol?uri=%s"))
          .AddHtml("/handle_protocol", "<h1>Handle Protocol</h1>")
          .BuildBundle();
  IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
  webapps::AppId app_id = url_info.app_id();

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");
  GURL protocol_url("web+test://test");

  base::RunLoop run_loop;
  auto dialog_finished =
      base::BindLambdaForTesting([&](bool allowed, bool remember_user_choice) {
        run_loop.Quit();
        EXPECT_FALSE(allowed);
      });

  ShowWebAppProtocolLaunchDialog(protocol_url, profile(), app_id,
                                 std::move(dialog_finished));

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  views::View* contents_view = widget->GetContentsView();
  EXPECT_TRUE(test::HasChildLabelWithSubstring(contents_view, u"3.4.5"));

  views::test::CancelDialog(widget);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogIwaTest,
                       LaunchSubAppShowsParentAppNameLabel) {
  auto parent_bundle =
      IsolatedWebAppBuilder(
          ManifestBuilder()
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
              }],
              "protocol_handlers": [{
                "protocol": "web+testsub",
                "url": "/subapp/handle_protocol?uri=%s"
              }]
            }
          )",
                       "application/manifest+json")
          .BuildBundle();
  IsolatedWebAppUrlInfo parent_url_info =
      parent_bundle->InstallChecked(profile());

  Browser* parent_browser =
      LaunchWebAppBrowserAndWait(parent_url_info.app_id());
  ASSERT_NE(parent_browser, nullptr);
  content::WebContents* parent_contents =
      parent_browser->tab_strip_model()->GetActiveWebContents();

  webapps::AppId sub_app_id = InstallSubAppAndWait(
      parent_contents, "/subapp/index.html",
      parent_url_info.origin().GetURL().Resolve("/subapp/index.html"));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");
  GURL protocol_url("web+testsub://test");

  base::RunLoop run_loop;
  auto dialog_finished =
      base::BindLambdaForTesting([&](bool allowed, bool remember_user_choice) {
        run_loop.Quit();
        EXPECT_FALSE(allowed);
      });

  ShowWebAppProtocolLaunchDialog(protocol_url, profile(), sub_app_id,
                                 std::move(dialog_finished));

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  views::View* contents_view = widget->GetContentsView();
  EXPECT_TRUE(test::HasChildLabelWithSubstring(contents_view, u"Parent IWA"));

  views::test::CancelDialog(widget);
  run_loop.Run();
}

}  // namespace

}  // namespace web_app
