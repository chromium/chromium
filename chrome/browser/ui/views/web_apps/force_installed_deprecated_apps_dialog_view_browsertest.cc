// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/views/web_apps/force_installed_preinstalled_deprecated_app_dialog_view.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
constexpr char kAppUrl[] = "https://www.app1.com/index.html";

constexpr char kMockAppManifest[] =
    "{"
    "  \"name\": \"Test App1\","
    "  \"version\": \"1\","
    "  \"manifest_version\": 2,"
    "  \"app\": {"
    "    \"launch\": {"
    "      \"web_url\": \"%s\""
    "    },"
    "    \"urls\": [\"*://app1.com/\"]"
    "  }"
    "}";

class TestAppLauncherHandler : public AppLauncherHandler {
 public:
  TestAppLauncherHandler(extensions::ExtensionService* extension_service,
                         web_app::WebAppProvider* web_app_provider,
                         content::TestWebUI* test_web_ui)
      : AppLauncherHandler(extension_service, web_app_provider) {
    DCHECK(test_web_ui->GetWebContents());
    DCHECK(test_web_ui->GetWebContents()->GetBrowserContext());
    set_web_ui(test_web_ui);
  }
};

class ForceInstalledDeprecatedAppsDialogViewBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  ForceInstalledDeprecatedAppsDialogViewBrowserTest() {
    bool disable_preinstalled_apps = GetParam();
    if (disable_preinstalled_apps) {
      feature_list_.InitWithFeatures(
          {features::kChromeAppsDeprecation},
          {features::kKeepForceInstalledPreinstalledApps});
    } else {
      feature_list_.InitWithFeatures(
          {features::kKeepForceInstalledPreinstalledApps,
           features::kChromeAppsDeprecation},
          {});
    }
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    app_id_ = InstallTestApp();
    // Install a test policy provider which will mark the app as
    // force-installed.
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(browser()->profile());
    extension_system->management_policy()->RegisterProvider(&policy_provider_);
  }

  TestAppLauncherHandler CreateLauncherHandler() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    DCHECK(web_contents);
    test_web_ui_.set_web_contents(web_contents);
    return TestAppLauncherHandler(
        extension_service(),
        web_app::WebAppProvider::GetForWebContents(web_contents),
        &test_web_ui_);
  }

  extensions::ExtensionId InstallTestApp() {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(
        base::StringPrintf(kMockAppManifest, GURL(kAppUrl).spec().c_str()));
    const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
        test_app_dir.UnpackedPath(), /*expected_change=*/1,
        extensions::mojom::ManifestLocation::kInternal,
        extensions::Extension::NO_FLAGS);
    DCHECK(app);
    return app->id();
  }

  extensions::ExtensionId app_id_;
  base::test::ScopedFeatureList feature_list_;
  extensions::TestManagementPolicyProvider policy_provider_{
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED};
  content::TestWebUI test_web_ui_{};
};

IN_PROC_BROWSER_TEST_P(ForceInstalledDeprecatedAppsDialogViewBrowserTest,
                       DialogLaunchedForForceInstalledApp) {
  auto handler = CreateLauncherHandler();

  base::Value::List input;
  input.Append(app_id_);
  input.Append(extension_misc::AppLaunchBucket::APP_LAUNCH_NTP_APPS_MENU);

  auto waiter =
      views::NamedWidgetShownWaiter(views::test::AnyWidgetTestPasskey{},
                                    "ForceInstalledDeprecatedAppsDialogView");
  handler.HandleLaunchApp(input);
  // Widget is shown.
  EXPECT_NE(waiter.WaitIfNeededAndGet(), nullptr);
}

IN_PROC_BROWSER_TEST_P(ForceInstalledDeprecatedAppsDialogViewBrowserTest,
                       DialogLaunchedForForceInstalledPreinstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Set app as a preinstalled app.
  extensions::SetPreinstalledAppIdForTesting(app_id_.c_str());
  auto link_config_reset = ForceInstalledPreinstalledDeprecatedAppDialogView::
      SetOverrideLinkConfigForTesting(
          {.link = GURL(embedded_test_server()->GetURL("/")),
           .link_text = u"www.example.com"});

  auto handler = CreateLauncherHandler();

  base::Value::List input;
  input.Append(app_id_);
  input.Append(extension_misc::AppLaunchBucket::APP_LAUNCH_NTP_APPS_MENU);
  bool disable_preinstalled_apps = GetParam();

  if (disable_preinstalled_apps) {
    auto waiter = views::NamedWidgetShownWaiter(
        views::test::AnyWidgetTestPasskey{},
        "ForceInstalledPreinstalledDeprecatedAppDialogView");
    handler.HandleLaunchApp(input);
    views::Widget* view = waiter.WaitIfNeededAndGet();
    // Widget is shown.
    EXPECT_NE(view, nullptr);
    ui_test_utils::UrlLoadObserver url_observer(
        embedded_test_server()->GetURL("/"),
        content::NotificationService::AllSources());
    views::test::AcceptDialog(view);
    url_observer.Wait();

  } else {
    ui_test_utils::UrlLoadObserver url_observer(
        GURL(kAppUrl), content::NotificationService::AllSources());
    handler.HandleLaunchApp(input);
    // Preinstalled chrome app is launched.
    url_observer.Wait();
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ForceInstalledDeprecatedAppsDialogViewBrowserTest,
                         testing::Bool());

}  // namespace
