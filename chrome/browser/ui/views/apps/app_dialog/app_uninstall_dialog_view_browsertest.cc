// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class AppUninstallDialogViewBrowserTest : public DialogBrowserTest {
 public:
  AppDialogView* ActiveView() {
    return AppUninstallDialogView::GetActiveViewForTesting();
  }

  void ShowUi(const std::string& name) override {
    EXPECT_EQ(nullptr, ActiveView());

    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(app_service_proxy);

    base::RunLoop run_loop;
    app_service_proxy->UninstallForTesting(app_id_, nullptr,
                                           run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_NE(nullptr, ActiveView());
    EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
              ActiveView()->GetDialogButtons());
    base::string16 title =
        base::ASCIIToUTF16("Uninstall \"" + app_name_ + "\"?");
    EXPECT_EQ(title, ActiveView()->GetWindowTitle());

    if (name == "accept") {
      ActiveView()->AcceptDialog();

      app_service_proxy->FlushMojoCallsForTesting();
      bool is_uninstalled = false;
      app_service_proxy->AppRegistryCache().ForOneApp(
          app_id_, [&is_uninstalled, name](const apps::AppUpdate& update) {
            is_uninstalled = (update.Readiness() ==
                              apps::mojom::Readiness::kUninstalledByUser);
          });

      EXPECT_TRUE(is_uninstalled);
    } else {
      ActiveView()->CancelDialog();

      app_service_proxy->FlushMojoCallsForTesting();
      bool is_installed = true;
      app_service_proxy->AppRegistryCache().ForOneApp(
          app_id_, [&is_installed, name](const apps::AppUpdate& update) {
            is_installed =
                (update.Readiness() == apps::mojom::Readiness::kReady);
          });

      EXPECT_TRUE(is_installed);
    }
    EXPECT_EQ(nullptr, ActiveView());
  }

 protected:
  std::string app_id_;
  std::string app_name_;
};

class ArcAppsUninstallDialogViewBrowserTest
    : public AppUninstallDialogViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    AppUninstallDialogViewBrowserTest::SetUpOnMainThread();

    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();

    arc_app_list_pref_ = ArcAppListPrefs::Get(browser()->profile());
    ASSERT_TRUE(arc_app_list_pref_);
    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  void CreateApp() {
    arc::mojom::AppInfo app;
    app.name = "Fake App 0";
    app.package_name = "fake.package.0";
    app.activity = "fake.app.0.activity";
    app.sticky = false;
    app_instance_->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(1, app));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1u, arc_app_list_pref_->GetAppIds().size());
    app_id_ = arc_app_list_pref_->GetAppId(app.package_name, app.activity);
    app_name_ = app.name;
  }

 private:
  ArcAppListPrefs* arc_app_list_pref_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(ArcAppsUninstallDialogViewBrowserTest, InvokeUi_Accept) {
  CreateApp();
  ShowUi("accept");
}

IN_PROC_BROWSER_TEST_F(ArcAppsUninstallDialogViewBrowserTest, InvokeUi_Cancel) {
  CreateApp();
  ShowUi("cancel");
}

class WebAppsUninstallDialogViewBrowserTest
    : public AppUninstallDialogViewBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AppUninstallDialogViewBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void CreateApp() {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();

    app_id_ =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    web_app::LaunchWebAppBrowser(browser()->profile(), app_id_);
    navigation_observer.WaitForNavigationFinished();

    auto* provider =
        web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
    DCHECK(provider);
    app_name_ = provider->registrar().GetAppShortName(app_id_);
  }

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

 protected:
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest, InvokeUi_Accept) {
  CreateApp();
  ShowUi("accept");
}

IN_PROC_BROWSER_TEST_F(WebAppsUninstallDialogViewBrowserTest, InvokeUi_Cancel) {
  CreateApp();
  ShowUi("cancel");
}
