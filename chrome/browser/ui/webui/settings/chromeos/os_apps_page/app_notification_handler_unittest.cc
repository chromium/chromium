// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

class FakeMessageCenterAsh : public ash::MessageCenterAsh {
 public:
  FakeMessageCenterAsh() = default;
  ~FakeMessageCenterAsh() override = default;

  // MessageCenterAsh override:
  void SetQuietMode(bool in_quiet_mode) override {
    if (in_quiet_mode != in_quiet_mode_) {
      in_quiet_mode_ = in_quiet_mode;
      NotifyOnQuietModeChanged(in_quiet_mode);
    }
  }

  bool IsQuietMode() const override { return in_quiet_mode_; }

 private:
  bool in_quiet_mode_ = false;
};

}  // namespace

class AppNotificationHandlerTest : public testing::Test {
 public:
  AppNotificationHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(std::make_unique<TestingProfile>()) {}
  ~AppNotificationHandlerTest() override = default;

  void SetUp() override {
    ash::MessageCenterAsh::SetForTesting(&message_center_ash_);
    app_service_proxy_ =
        std::make_unique<apps::AppServiceProxyChromeOs>(profile_.get());
    handler_ =
        std::make_unique<AppNotificationHandler>(app_service_proxy_.get());
  }

  void TearDown() override {
    handler_.reset();
    app_service_proxy_.reset();
    ash::MessageCenterAsh::SetForTesting(nullptr);
  }

 protected:
  bool GetHandlerQuietModeState() { return handler_->in_quiet_mode_; }

  void SetQuietModeState(bool quiet_mode_enabled) {
    handler_->SetQuietMode(quiet_mode_enabled);
  }

  void CreateAndStoreFakeApp(
      std::string fake_id,
      apps::mojom::AppType app_type,
      std::uint32_t permission_type,
      apps::mojom::PermissionValueType permission_value_type) {
    std::vector<apps::mojom::PermissionPtr> fake_permissions;
    apps::mojom::PermissionPtr fake_permission = apps::mojom::Permission::New();
    fake_permission->permission_id = permission_type;
    fake_permission->value_type = permission_value_type;
    fake_permission->value = /*True=*/1;
    fake_permission->is_managed = false;

    fake_permissions.push_back(fake_permission.Clone());

    std::vector<apps::mojom::AppPtr> fake_apps;
    apps::mojom::AppPtr fake_app = apps::mojom::App::New();
    fake_app->app_type = app_type;
    fake_app->app_id = fake_id;
    fake_app->show_in_management = apps::mojom::OptionalBool::kTrue;
    fake_app->readiness = apps::mojom::Readiness::kReady;
    fake_app->permissions = std::move(fake_permissions);

    fake_apps.push_back(fake_app.Clone());

    UpdateAppRegistryCache(fake_apps, app_type);
  }

  void UpdateAppRegistryCache(std::vector<apps::mojom::AppPtr>& fake_apps,
                              apps::mojom::AppType app_type) {
    app_service_proxy_->AppRegistryCache().OnApps(std::move(fake_apps),
                                                  app_type, false);
  }

  bool CheckIfFakeAppInList(std::string fake_id) {
    bool app_found = false;

    for (app_notification::mojom::AppPtr const& app : handler_->apps_) {
      if (app->id.compare(fake_id) == 0) {
        app_found = true;
        break;
      }
    }
    return app_found;
  }

 private:
  std::unique_ptr<AppNotificationHandler> handler_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<apps::AppServiceProxyChromeOs> app_service_proxy_;
  FakeMessageCenterAsh message_center_ash_;
};

// Tests for update of in_quiet_mode_ variable by MessageCenterAsh observer
// OnQuietModeChange() after quiet mode state change between true and false.
TEST_F(AppNotificationHandlerTest, TestOnQuietModeChanged) {
  ash::MessageCenterAsh::Get()->SetQuietMode(true);
  EXPECT_TRUE(GetHandlerQuietModeState());

  ash::MessageCenterAsh::Get()->SetQuietMode(false);
  EXPECT_FALSE(GetHandlerQuietModeState());
}

// Tests for update of in_quiet_mode_ variable after setting state
// with MessageCenterAsh SetQuietMode() true and false.
TEST_F(AppNotificationHandlerTest, TestSetQuietMode) {
  SetQuietModeState(true);
  EXPECT_TRUE(GetHandlerQuietModeState());

  SetQuietModeState(false);
  EXPECT_FALSE(GetHandlerQuietModeState());
}

// Tests the filtering of the GetApps() function
// by creating multiple fake apps with different parameters
// and confirming that GetApps() only adds the correct ones.
// GetApps() should only add kArc and kWeb apps
// with the NOTIFICATIONS permission.
TEST_F(AppNotificationHandlerTest, TestGetAppsFiltering) {
  CreateAndStoreFakeApp(
      "arcAppWithNotifications", apps::mojom::AppType::kArc,
      static_cast<std::uint32_t>(
          app_management::mojom::ArcPermissionType::NOTIFICATIONS),
      apps::mojom::PermissionValueType::kBool);

  CreateAndStoreFakeApp(
      "webAppWithNotifications", apps::mojom::AppType::kWeb,
      static_cast<std::uint32_t>(
          app_management::mojom::PwaPermissionType::NOTIFICATIONS),
      apps::mojom::PermissionValueType::kBool);

  CreateAndStoreFakeApp("arcAppWithCamera", apps::mojom::AppType::kArc,
                        static_cast<std::uint32_t>(
                            app_management::mojom::ArcPermissionType::CAMERA),
                        apps::mojom::PermissionValueType::kBool);

  CreateAndStoreFakeApp(
      "webAppWithGeolocation", apps::mojom::AppType::kWeb,
      static_cast<std::uint32_t>(
          app_management::mojom::PwaPermissionType::GEOLOCATION),
      apps::mojom::PermissionValueType::kBool);

  CreateAndStoreFakeApp(
      "pluginVmAppWithPrinting", apps::mojom::AppType::kPluginVm,
      static_cast<std::uint32_t>(
          app_management::mojom::PluginVmPermissionType::PRINTING),
      apps::mojom::PermissionValueType::kBool);

  EXPECT_TRUE(CheckIfFakeAppInList("arcAppWithNotifications"));
  EXPECT_TRUE(CheckIfFakeAppInList("webAppWithNotifications"));
  EXPECT_FALSE(CheckIfFakeAppInList("arcAppWithCamera"));
  EXPECT_FALSE(CheckIfFakeAppInList("webAppWithGeolocation"));
  EXPECT_FALSE(CheckIfFakeAppInList("pluginVmAppWithPrinting"));
}

}  // namespace settings
}  // namespace chromeos
