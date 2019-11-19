// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include "base/barrier_closure.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#endif

namespace web_app {

namespace {

// Waits for |browser| to be removed from BrowserList and then calls |callback|.
class BrowserRemovedWaiter final : public BrowserListObserver {
 public:
  explicit BrowserRemovedWaiter(Browser* browser) : browser_(browser) {}
  ~BrowserRemovedWaiter() override = default;

  void Wait() {
    BrowserList::AddObserver(this);
    run_loop_.Run();
  }

  // BrowserListObserver
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_)
      return;

    BrowserList::RemoveObserver(this);
    // Post a task to ensure the Remove event has been dispatched to all
    // observers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 private:
  Browser* browser_;
  base::RunLoop run_loop_;
};

void CloseAndWait(Browser* browser) {
  BrowserRemovedWaiter waiter(browser);
  browser->window()->Close();
  waiter.Wait();
}

}  // namespace

const GURL kFooUrl = GURL("https://foo.example");
const GURL kBarUrl = GURL("https://bar.example");

class WebAppUiManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  Profile* profile() { return browser()->profile(); }

  const AppId InstallWebApp(const GURL& app_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = app_url;
    web_app_info->open_as_window = true;
    return web_app::InstallWebApp(profile(), std::move(web_app_info));
  }

  Browser* LaunchWebApp(const AppId& app_id) {
    return LaunchWebAppBrowser(profile(), app_id);
  }

  WebAppUiManager& ui_manager() {
    return WebAppProviderBase::GetProviderBase(profile())->ui_manager();
  }
};

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsAdded) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(AppId()));

  AppId foo_app_id = InstallWebApp(kFooUrl);
  LaunchWebApp(foo_app_id);
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));

  LaunchWebApp(foo_app_id);
  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsRemoved) {
  AppId foo_app_id = InstallWebApp(kFooUrl);
  auto* foo_window1 = LaunchWebApp(foo_app_id);
  auto* foo_window2 = LaunchWebApp(foo_app_id);

  AppId bar_app_id = InstallWebApp(kBarUrl);
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
  AppId foo_app_id = InstallWebApp(kFooUrl);
  AppId bar_app_id = InstallWebApp(kBarUrl);
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
  AppId foo_app_id = InstallWebApp(kFooUrl);
  AppId bar_app_id = InstallWebApp(kBarUrl);

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

#if defined(OS_CHROMEOS)
class WebAppUiManagerMigrationBrowserTest
    : public WebAppUiManagerImplBrowserTest {
 public:
  void SetUp() override {
    hide_settings_app_for_testing_ =
        apps::BuiltInChromeOsApps::SetHideSettingsAppForTesting(true);
    // Disable System Web Apps so that the Internal Apps are installed.
    scoped_feature_list_.InitAndDisableFeature(features::kSystemWebApps);
    WebAppUiManagerImplBrowserTest::SetUp();
  }

  void TearDown() override {
    WebAppUiManagerImplBrowserTest::TearDown();
    apps::BuiltInChromeOsApps::SetHideSettingsAppForTesting(
        hide_settings_app_for_testing_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool hide_settings_app_for_testing_ = false;
};

// Tests that the Settings app migrates the launcher and app list details from
// the Settings internal app.
//
// TODO(https://crbug.com/1012967): Find a way to implement this that does not
// depend on unsupported behavior of the FeatureList API.
IN_PROC_BROWSER_TEST_F(WebAppUiManagerMigrationBrowserTest,
                       DISABLED_SettingsSystemWebAppMigration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kSystemWebApps);

  auto& system_web_app_manager =
      WebAppProvider::Get(browser()->profile())->system_web_app_manager();

  auto* app_list_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(
          browser()->profile());

  // Pin the Settings Internal App.
  syncer::StringOrdinal pin_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  pin_position = pin_position.CreateAfter().CreateAfter();
  app_list_service->SetPinPosition(ash::kInternalAppIdSettings, pin_position);

  // Add the Settings Internal App to a folder.
  AppListModelUpdater* updater =
      test::GetModelUpdater(test::GetAppListClient());
  updater->MoveItemToFolder(ash::kInternalAppIdSettings, "asdf");

  // Install the Settings System Web App, which should be immediately migrated
  // to the Settings Internal App's details.
  system_web_app_manager.InstallSystemAppsForTesting();
  std::string settings_system_web_app_id =
      *system_web_app_manager.GetAppIdForSystemApp(SystemAppType::SETTINGS);
  {
    const app_list::AppListSyncableService::SyncItem* web_app_item =
        app_list_service->GetSyncItem(settings_system_web_app_id);
    const app_list::AppListSyncableService::SyncItem* internal_app_item =
        app_list_service->GetSyncItem(ash::kInternalAppIdSettings);

    EXPECT_TRUE(internal_app_item->item_pin_ordinal.Equals(
        web_app_item->item_pin_ordinal));
    EXPECT_TRUE(
        internal_app_item->item_ordinal.Equals(web_app_item->item_ordinal));
    EXPECT_EQ(internal_app_item->parent_id, web_app_item->parent_id);
  }

  // Change Settings System Web App properties.
  app_list_service->SetPinPosition(
      settings_system_web_app_id,
      syncer::StringOrdinal::CreateInitialOrdinal());
  updater->MoveItemToFolder(settings_system_web_app_id, std::string());

  // Do migration again with the already-installed app. Should be a no-op.
  system_web_app_manager.InstallSystemAppsForTesting();
  {
    const app_list::AppListSyncableService::SyncItem* web_app_item =
        app_list_service->GetSyncItem(settings_system_web_app_id);

    EXPECT_TRUE(syncer::StringOrdinal::CreateInitialOrdinal().Equals(
        web_app_item->item_pin_ordinal));
    EXPECT_EQ(std::string(), web_app_item->parent_id);
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace web_app
