// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_test_base.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

class AppParentalControlsHandlerTest
    : public on_device_controls::AppControlsTestBase {
 public:
  AppParentalControlsHandlerTest() {}
  ~AppParentalControlsHandlerTest() override = default;

  void SetUp() override {
    on_device_controls::AppControlsTestBase::SetUp();

    handler_ = std::make_unique<AppParentalControlsHandler>(
        app_service_test().proxy(), profile().GetPrefs());
  }

  void TearDown() override {
    handler_.reset();

    on_device_controls::AppControlsTestBase::TearDown();
  }

 protected:
  void CreateAndStoreFakeApp(std::string fake_id,
                             apps::AppType app_type,
                             bool shown_in_management) {
    std::vector<apps::AppPtr> fake_apps;
    apps::AppPtr fake_app = std::make_unique<apps::App>(app_type, fake_id);
    fake_app->show_in_management = shown_in_management;
    fake_app->readiness = apps::Readiness::kReady;
    fake_apps.push_back(std::move(fake_app));

    UpdateAppRegistryCache(fake_apps, app_type);
  }

  void UpdateAppRegistryCache(std::vector<apps::AppPtr>& fake_apps,
                              apps::AppType app_type) {
    app_service_test().proxy()->OnApps(std::move(fake_apps), app_type, false);
  }

 protected:
  std::unique_ptr<AppParentalControlsHandler> handler_;
};

TEST_F(AppParentalControlsHandlerTest, TestOnlyManageableArcAppsFetched) {
  CreateAndStoreFakeApp("arcApp1", apps::AppType::kArc,
                        /*shown_in_management=*/true);
  CreateAndStoreFakeApp("webApp", apps::AppType::kWeb,
                        /*shown_in_management=*/true);
  CreateAndStoreFakeApp("arcApp2", apps::AppType::kArc,
                        /*shown_in_management=*/true);
  CreateAndStoreFakeApp("unmanageableArcApp", apps::AppType::kArc,
                        /*shown_in_management=*/false);

  base::RunLoop run_loop;
  handler_->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 2u);
        EXPECT_EQ(apps[0]->id, "arcApp1");
        EXPECT_EQ(apps[1]->id, "arcApp2");
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(AppParentalControlsHandlerTest, TesAppUpdate) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());

  base::RunLoop run_loop1;
  handler_->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, app_id);
        EXPECT_FALSE(apps[0]->is_blocked);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  handler_->UpdateApp(app_id, /*is_blocked=*/true);

  base::RunLoop run_loop2;
  handler_->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, app_id);
        EXPECT_TRUE(apps[0]->is_blocked);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  handler_->UpdateApp(app_id, /*is_blocked=*/false);

  base::RunLoop run_loop3;
  handler_->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, app_id);
        EXPECT_FALSE(apps[0]->is_blocked);
        run_loop3.Quit();
      }));
  run_loop3.Run();
}

}  // namespace ash::settings
