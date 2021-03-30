// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shell.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

void UpdateAppRegistryCache(Profile* profile,
                            const std::string& app_id,
                            bool block,
                            bool pause) {
  std::vector<apps::mojom::AppPtr> apps;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = app_id;

  if (block)
    app->readiness = apps::mojom::Readiness::kDisabledByPolicy;
  else
    app->readiness = apps::mojom::Readiness::kReady;

  if (pause)
    app->paused = apps::mojom::OptionalBool::kTrue;
  else
    app->paused = apps::mojom::OptionalBool::kFalse;

  apps.push_back(std::move(app));

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::mojom::AppType::kExtension,
              false /* should_notify_initialized */);
}

ash::AppListItem* GetAppListItem(const std::string& id) {
  ash::AppListControllerImpl* controller =
      ash::Shell::Get()->app_list_controller();
  ash::AppListModel* model = controller->GetModel();
  return model->FindItem(id);
}

}  // namespace

class AppServiceAppItemBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  AppServiceAppItemBrowserTest() = default;
  ~AppServiceAppItemBrowserTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);

    // Associate |client| with the current profile.
    client->UpdateProfile();
  }
};

// Test the app status when the paused app is blocked, un-paused, and un-blocked
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForPausedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kPaused, item->app_status());

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as blocked, but not paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}

// Test the app status when the blocked app is paused, un-blocked, and un-paused
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForBlockedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as blocked.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as not blocked, but paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kPaused, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}

// Test the app status when the app is both blocked and paused.
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForBlockedAndPausedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}
