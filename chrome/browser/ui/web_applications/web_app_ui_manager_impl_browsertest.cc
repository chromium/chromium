// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#endif

namespace web_app {

class WebAppUiManagerImplBrowserTest : public InProcessBrowserTest {
 public:
  WebAppUiManagerImplBrowserTest()
      : fake_web_app_provider_creator_(base::BindRepeating(
            &WebAppUiManagerImplBrowserTest::CreateFakeWebAppProvider,
            base::Unretained(this))) {}

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

  Profile* profile() { return browser()->profile(); }

  AppId InstallWebApp(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->user_display_mode = UserDisplayMode::kStandalone;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  Browser* LaunchWebApp(const AppId& app_id) {
    return LaunchWebAppBrowser(profile(), app_id);
  }

  WebAppUiManager& ui_manager() {
    return WebAppProvider::GetForTest(profile())->ui_manager();
  }

  raw_ptr<TestShortcutManager, DanglingUntriaged> shortcut_manager_;
  raw_ptr<FakeOsIntegrationManager, DanglingUntriaged> os_integration_manager_;

 private:
  std::unique_ptr<KeyedService> CreateFakeWebAppProvider(Profile* profile) {
    auto provider = std::make_unique<FakeWebAppProvider>(profile);
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile);
    shortcut_manager_ = shortcut_manager.get();
    auto os_integration_manager = std::make_unique<FakeOsIntegrationManager>(
        profile, std::move(shortcut_manager), nullptr, nullptr, nullptr);
    os_integration_manager_ = os_integration_manager.get();
    provider->SetOsIntegrationManager(std::move(os_integration_manager));
    provider->Start();
    DCHECK(provider);
    return provider;
  }

  FakeWebAppProviderCreator fake_web_app_provider_creator_;
};

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsAdded) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(AppId()));

  AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  LaunchWebApp(foo_app_id);
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));

  LaunchWebApp(foo_app_id);
  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       UninstallDuringLastBrowserWindow) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(AppId()));
  AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  LaunchWebApp(foo_app_id);
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));
  // It has 2 browser window object.
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  web_app::CloseAndWait(browser());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  BrowserWaiter waiter(app_browser);
  // Uninstalling should close the |app_browser|, but keep the browser
  // object alive long enough to complete the uninstall.
  test::UninstallWebApp(app_browser->profile(), foo_app_id);
  waiter.AwaitRemoved();

  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsRemoved) {
  AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  auto* foo_window1 = LaunchWebApp(foo_app_id);
  auto* foo_window2 = LaunchWebApp(foo_app_id);

  AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));
  LaunchWebApp(bar_app_id);

  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));

  CloseAndWait(foo_window1);

  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));

  CloseAndWait(foo_window2);

  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       NotifyOnAllAppWindowsClosed_NoOpenedWindows) {
  AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));
  LaunchWebApp(bar_app_id);

  base::RunLoop run_loop;
  // Should return early; no windows for |foo_app|.
  ui_manager().NotifyOnAllAppWindowsClosed(foo_app_id, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that the callback is correctly called when there is more than one
// app window.
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       NotifyOnAllAppWindowsClosed_MultipleOpenedWindows) {
  AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));

  // Test that NotifyOnAllAppWindowsClosed can be called more than once for
  // the same app.
  for (int i = 0; i < 2; i++) {
    auto* foo_window1 = LaunchWebApp(foo_app_id);
    auto* foo_window2 = LaunchWebApp(foo_app_id);
    auto* bar_window = LaunchWebApp(bar_app_id);

    bool callback_ran = false;
    base::RunLoop run_loop;
    ui_manager().NotifyOnAllAppWindowsClosed(foo_app_id,
                                             base::BindLambdaForTesting([&]() {
                                               callback_ran = true;
                                               run_loop.Quit();
                                             }));

    CloseAndWait(foo_window1);
    // The callback shouldn't have run yet because there is still one window
    // opened.
    EXPECT_FALSE(callback_ran);

    CloseAndWait(bar_window);
    EXPECT_FALSE(callback_ran);

    CloseAndWait(foo_window2);
    run_loop.Run();
    EXPECT_TRUE(callback_ran);
  }
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Regression test for crbug.com/1182030
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       WebAppMigrationPreservesShortcutStates) {
  const GURL kOldAppUrl("https://old.app.com");
  // Install an old app to be replaced.
  AppId old_app_id = InstallWebApp(kOldAppUrl);

  // Set up the existing shortcuts.
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->url = kOldAppUrl;
  shortcut_manager_->SetShortcutInfoForApp(old_app_id,
                                           std::move(shortcut_info));
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  shortcut_manager_->SetAppExistingShortcuts(kOldAppUrl, locations);

  // Install a new app to migrate the old one to.
  AppId new_app_id = InstallWebApp(GURL("https://new.app.com"));
  ui_manager().UninstallAndReplaceIfExists({old_app_id}, new_app_id);

  EXPECT_TRUE(os_integration_manager_->did_add_to_desktop());
  auto options = os_integration_manager_->get_last_install_options();
  EXPECT_TRUE(options->os_hooks[OsHookType::kRunOnOsLogin]);
  EXPECT_FALSE(options->add_to_quick_launch_bar);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that app migrations use the UI preferences of the replaced app but only
// if it's present.
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest, DoubleMigration) {
  app_list::AppListSyncableService* app_list_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(
          browser()->profile());

  // Install an old app to be replaced.
  AppId old_app_id = InstallWebApp(GURL("https://old.app.com"));
  app_list_service->SetPinPosition(old_app_id,
                                   syncer::StringOrdinal("positionold"));

  // Install a new app to migrate the old one to.
  AppId new_app_id = InstallWebApp(GURL("https://new.app.com"));
  {
    WebAppTestUninstallObserver waiter(browser()->profile());
    waiter.BeginListening({old_app_id});
    ui_manager().UninstallAndReplaceIfExists({old_app_id}, new_app_id);
    waiter.Wait();
  }

  // New app should acquire old app's pin position.
  EXPECT_EQ(app_list_service->GetSyncItem(new_app_id)
                ->item_pin_ordinal.ToDebugString(),
            "positionold");

  // Change the new app's pin position.
  app_list_service->SetPinPosition(new_app_id,
                                   syncer::StringOrdinal("positionnew"));

  // Do migration again. New app should not move.
  ui_manager().UninstallAndReplaceIfExists({old_app_id}, new_app_id);
  EXPECT_EQ(app_list_service->GetSyncItem(new_app_id)
                ->item_pin_ordinal.ToDebugString(),
            "positionnew");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
