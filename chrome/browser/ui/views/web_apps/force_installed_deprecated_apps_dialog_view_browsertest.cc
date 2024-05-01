// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/views/web_apps/force_installed_preinstalled_deprecated_app_dialog_view.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

class ForceInstalledDeprecatedAppsDialogViewBrowserTest
    : public extensions::ExtensionBrowserTest {
 protected:
  ForceInstalledDeprecatedAppsDialogViewBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kChromeAppsDeprecation);
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

  webapps::AppHomePageHandler CreateLauncherHandler(
      content::TestWebUI* web_ui) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    DCHECK(web_contents);
    test_web_ui_.set_web_contents(web_contents);
    mojo::PendingReceiver<app_home::mojom::Page> page;
    mojo::Remote<app_home::mojom::PageHandler> page_handler;
    return webapps::AppHomePageHandler(
        web_ui, profile(), page_handler.BindNewPipeAndPassReceiver(),
        page.InitWithNewPipeAndPassRemote());
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

IN_PROC_BROWSER_TEST_F(ForceInstalledDeprecatedAppsDialogViewBrowserTest,
                       DialogLaunchedForForceInstalledApp) {
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto handler = CreateLauncherHandler(&test_web_ui);

  auto waiter =
      views::NamedWidgetShownWaiter(views::test::AnyWidgetTestPasskey{},
                                    "ForceInstalledDeprecatedAppsDialogView");
  handler.LaunchApp(app_id_, app_home::mojom::ClickEventPtr());
  // Widget is shown.
  EXPECT_NE(waiter.WaitIfNeededAndGet(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ForceInstalledDeprecatedAppsDialogViewBrowserTest,
                       DialogLaunchedForForceInstalledPreinstalledApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Set app as a preinstalled app.
  extensions::SetPreinstalledAppIdForTesting(app_id_.c_str());
  auto link_config_reset = ForceInstalledPreinstalledDeprecatedAppDialogView::
      SetOverrideLinkConfigForTesting(
          {.link = GURL(embedded_test_server()->GetURL("/")),
           .link_text = u"www.example.com",
           // We use a filler value here. This is only used as input to
           // histograms.
           .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
               kGmail});

  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto handler = CreateLauncherHandler(&test_web_ui);

  auto waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{},
      "ForceInstalledPreinstalledDeprecatedAppDialogView");
  handler.LaunchApp(app_id_, app_home::mojom::ClickEventPtr());
  views::Widget* view = waiter.WaitIfNeededAndGet();
  // Widget is shown.
  EXPECT_NE(view, nullptr);
  ui_test_utils::UrlLoadObserver url_observer(
      embedded_test_server()->GetURL("/"));
  views::test::AcceptDialog(view);
  url_observer.Wait();
}

}  // namespace
