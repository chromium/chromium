// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/extension_registry.h"

namespace web_app {

class AppBrowserControllerBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  AppBrowserControllerBrowserTest() {
#if defined(OS_CHROMEOS)
    scoped_feature_list_.InitWithFeatures({}, {features::kTerminalSystemApp});
#endif
  }

  ~AppBrowserControllerBrowserTest() override {}

  void SetUp() override { extensions::ExtensionBrowserTest::SetUp(); }

 protected:
  void InstallAndLaunchTerminalApp() {
    GURL app_url = GetAppURL();
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = app_url;
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app_info->open_as_window = true;
    AppId app_id = InstallWebApp(std::move(web_app_info));

    auto* provider = WebAppProvider::Get(profile());
    provider->system_web_app_manager().SetSystemAppsForTesting(
        {{SystemAppType::TERMINAL, SystemAppInfo(app_url)}});
    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(app_url, app_id, ExternalInstallSource::kInternalDefault);
    ASSERT_EQ(
        GetAppIdForSystemWebApp(browser()->profile(), SystemAppType::TERMINAL),
        app_id);

    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
            app_id);
    ASSERT_TRUE(extension);

    app_browser_ = LaunchAppBrowser(extension);

    ASSERT_TRUE(app_browser_);
    ASSERT_NE(app_browser_, browser());
  }

  std::string InstallWebApp(
      std::unique_ptr<WebApplicationInfo>&& web_app_info) {
    std::string app_id;
    base::RunLoop run_loop;
    auto* provider = WebAppProvider::Get(profile());
    DCHECK(provider);
    provider->install_manager().InstallWebAppFromInfo(
        std::move(web_app_info), ForInstallableSite::kYes,
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&](const std::string& installed_app_id, InstallResultCode code) {
              EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
              app_id = installed_app_id;
              run_loop.Quit();
            }));

    run_loop.Run();
    return app_id;
  }

  GURL GetAppURL() {
    return embedded_test_server()->GetURL("app.com", "/simple.html");
  }

  GURL GetActiveTabURL() {
    return app_browser_->tab_strip_model()
        ->GetActiveWebContents()
        ->GetVisibleURL();
  }

  Browser* app_browser_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppBrowserControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppBrowserControllerBrowserTest, TabsTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  InstallAndLaunchTerminalApp();

  EXPECT_TRUE(app_browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP));

  // Check URL of tab1.
  EXPECT_EQ(GetActiveTabURL(), GetAppURL());
  // Create tab2 with specific URL, check URL, number of tabs.
  chrome::AddTabAt(app_browser_, GURL(chrome::kChromeUINewTabURL), -1, true);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Create tab3 with default URL, check URL, number of tabs.
  chrome::NewTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), GetAppURL());
  // Switch to tab1, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), GetAppURL());
  // Switch to tab2, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Switch to tab3, check URL.
  chrome::SelectNextTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 3);
  EXPECT_EQ(GetActiveTabURL(), GetAppURL());
  // Close tab3, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 2);
  EXPECT_EQ(GetActiveTabURL(), GURL("chrome://newtab/"));
  // Close tab2, check number of tabs.
  chrome::CloseTab(app_browser_);
  EXPECT_EQ(app_browser_->tab_strip_model()->count(), 1);
  EXPECT_EQ(GetActiveTabURL(), GetAppURL());
}

}  // namespace web_app
