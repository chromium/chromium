// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#endif

namespace web_app {

class WebAppUiManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
        WebAppProvider::GetForTest(profile()));
  }

  Profile* profile() { return browser()->profile(); }

  webapps::AppId InstallWebApp(const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  Browser* LaunchWebApp(const webapps::AppId& app_id) {
    return LaunchWebAppBrowser(profile(), app_id);
  }

  WebAppUiManager& ui_manager() {
    return WebAppProvider::GetForTest(profile())->ui_manager();
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsAdded) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(webapps::AppId()));

  webapps::AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  LaunchWebApp(foo_app_id);
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));

  LaunchWebApp(foo_app_id);
  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       UninstallDuringLastBrowserWindow) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(webapps::AppId()));
  webapps::AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
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
  webapps::AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  auto* foo_window1 = LaunchWebApp(foo_app_id);
  auto* foo_window2 = LaunchWebApp(foo_app_id);

  webapps::AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));
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
  webapps::AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  webapps::AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));
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
  webapps::AppId foo_app_id = InstallWebApp(GURL("https://foo.example"));
  webapps::AppId bar_app_id = InstallWebApp(GURL("https://bar.example"));

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest, MigrateAppAttribute) {
  app_list::AppListSyncableService* app_list_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(
          browser()->profile());

  // Install an old app to be replaced.
  webapps::AppId old_app_id = test::InstallDummyWebApp(
      profile(), "old_app", GURL("https://old.app.com"));
  app_list_service->SetPinPosition(old_app_id,
                                   syncer::StringOrdinal("positionold"),
                                   /*pinned_by_policy=*/false);

  // Install a new app to migrate the old one to.
  webapps::AppId new_app_id = test::InstallDummyWebApp(
      profile(), "new_app", GURL("https://new.app.com"));
  base::test::TestFuture<void> future;
  ui_manager().MigrateLauncherState(old_app_id, new_app_id,
                                    future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // New app should acquire old app's pin position.
  EXPECT_EQ(app_list_service->GetSyncItem(new_app_id)
                ->item_pin_ordinal.ToDebugString(),
            "positionold");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
