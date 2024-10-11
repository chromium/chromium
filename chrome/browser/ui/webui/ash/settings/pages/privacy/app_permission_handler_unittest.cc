// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/app_permission_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/eche_app/app_id.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/mojom/app_permission_handler.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

class AppUpdateWaiter {
 public:
  AppUpdateWaiter() {}
  ~AppUpdateWaiter() {}

  void SetUp(const std::string& app_id) {
    condition_met_ = false;
    app_id_ = app_id;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void Wait() {
    if (!condition_met_) {
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  void MaybeStop(const std::string& app_id) {
    if (app_id != app_id_) {
      return;
    }
    if (run_loop_->running()) {
      run_loop_->Quit();
    } else {
      run_loop_.reset();
    }
    condition_met_ = true;
  }

 private:
  bool condition_met_;
  std::string app_id_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class AppPermissionHandlerTestObserver
    : public app_permission::mojom::AppPermissionsObserver {
 public:
  AppPermissionHandlerTestObserver() {}
  ~AppPermissionHandlerTestObserver() override {}

  // app_permission::mojom::AppPermissionsObserver
  void OnAppUpdated(app_permission::mojom::AppPtr app) override {
    app_update_count_++;
    waiter_.MaybeStop(app->id);
  }
  void OnAppRemoved(const std::string& app_id) override {
    app_uninstall_count_++;
    waiter_.MaybeStop(app_id);
  }

  mojo::PendingRemote<app_permission::mojom::AppPermissionsObserver>
  GenerateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Configures the `waiter_` object to wait for an update of the app identified
  // by `app_id`. `WaitForAppUpdate()` has to be called after this to actually
  // start the `RunLoop`.
  void SetUpWaiterForAppUpdate(const std::string& app_id) {
    waiter_.SetUp(app_id);
  }

  // `SetUpWaiterForAppUpdate()` must be called before calling
  // `WaitForAppUpdate()`. The behaviour can be unpredictable otherwise.
  void WaitForAppUpdate() { waiter_.Wait(); }

  int app_update_count() { return app_update_count_; }
  int app_uninstall_count() { return app_uninstall_count_; }

 private:
  int app_update_count_ = 0;
  int app_uninstall_count_ = 0;

  AppUpdateWaiter waiter_;

  mojo::Receiver<app_permission::mojom::AppPermissionsObserver> receiver_{this};
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class AppPermissionHandlerTest : public testing::Test {
 public:
  AppPermissionHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(std::make_unique<TestingProfile>()) {}
  ~AppPermissionHandlerTest() override = default;

  void SetUp() override {
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    apps::WaitForAppServiceProxyReady(app_service_proxy_.get());
    handler_ = std::make_unique<AppPermissionHandler>(app_service_proxy_.get());

    observer_ = std::make_unique<AppPermissionHandlerTestObserver>();
    handler_->AddObserver(observer_->GenerateRemote());
  }

  void TearDown() override {
    handler_.reset();
  }

 protected:
  AppPermissionHandlerTestObserver* observer() { return observer_.get(); }

  void InstallApp(
      const std::string& app_id,
      const std::vector<std::pair<apps::PermissionType, bool>>& permissions,
      bool wait_for_update_to_propagate = true) {
    UpdateApp(app_id, permissions, apps::Readiness::kReady,
              wait_for_update_to_propagate);
  }

  void UninstallApp(const std::string& app_id,
                    bool wait_for_update_to_propagate = true) {
    UpdateApp(app_id, {}, apps::Readiness::kUninstalledByUser,
              wait_for_update_to_propagate);
  }

  // If there are no changes to be made to the state of permissions, empty
  // vector should be passed as the second argument.
  // If there are no changes to be made to the readiness state of the app,
  // `absl::nulllopt` should be passed as the third argument.
  void UpdateApp(const std::string& app_id,
                 const std::vector<std::pair<apps::PermissionType, bool>>&
                     permission_updates,
                 std::optional<apps::Readiness> readiness,
                 bool wait_for_update_to_propagate = true) {
    if (wait_for_update_to_propagate) {
      observer_->SetUpWaiterForAppUpdate(app_id);
    }

    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);

    std::vector<apps::PermissionPtr> permissions;
    for (const auto& permission_update : permission_updates) {
      apps::PermissionPtr permission = std::make_unique<apps::Permission>(
          apps::PermissionType{permission_update.first},
          apps::Permission::PermissionValue{permission_update.second},
          /*is_managed=*/false);
      permissions.push_back(std::move(permission));
    }
    app->permissions = std::move(permissions);
    app->readiness = readiness.value_or(app->readiness);
    app->show_in_management = true;

    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));
    app_service_proxy_->OnApps(std::move(apps), apps::AppType::kWeb, false);

    if (wait_for_update_to_propagate) {
      observer_->WaitForAppUpdate();
    }
  }

  int GetNumberOfInstalledApps() { return handler_->GetAppList().size(); }

  int GetNumberOfSystemAppsThatUseCamera() {
    return handler_->GetSystemAppListThatUsesCamera().size();
  }

  int GetNumberOfSystemAppsThatUseMicrophone() {
    return handler_->GetSystemAppListThatUsesMicrophone().size();
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  void OpenBrowserPermissionSettings(apps::PermissionType permission_type) {
    handler_->OpenBrowserPermissionSettings(permission_type);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AppPermissionHandler> handler_;
  std::unique_ptr<AppPermissionHandlerTestObserver> observer_;
  raw_ptr<apps::AppServiceProxy> app_service_proxy_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(AppPermissionHandlerTest, InstallApp) {
  InstallApp("appWithMicrophonePermission",
             {std::make_pair(apps::PermissionType::kMicrophone, true)});
  EXPECT_EQ(1, observer()->app_update_count());

  InstallApp("appWithCameraPermission",
             {std::make_pair(apps::PermissionType::kCamera, true)});
  EXPECT_EQ(2, observer()->app_update_count());
}

TEST_F(AppPermissionHandlerTest, UpdateExistingApp) {
  const std::string app_id = "appWithMicrophonePermission";

  InstallApp(app_id, {std::make_pair(apps::PermissionType::kMicrophone, true)});
  EXPECT_EQ(1, observer()->app_update_count());

  // Update the microphone permission.
  UpdateApp(app_id, {std::make_pair(apps::PermissionType::kMicrophone, false)},
            std::nullopt);
  EXPECT_EQ(2, observer()->app_update_count());
}

TEST_F(AppPermissionHandlerTest, UninstallApp) {
  const std::string app_id = "appWithCameraPermission";

  InstallApp(app_id, {std::make_pair(apps::PermissionType::kCamera, true)});
  EXPECT_EQ(0, observer()->app_uninstall_count());

  // Update the camera permission.
  UpdateApp(app_id, {std::make_pair(apps::PermissionType::kCamera, false)},
            std::nullopt);
  EXPECT_EQ(0, observer()->app_uninstall_count());

  UninstallApp(app_id);
  EXPECT_EQ(1, observer()->app_uninstall_count());
}

TEST_F(AppPermissionHandlerTest, GetAppList) {
  InstallApp("appWithCameraPermission",
             {std::make_pair(apps::PermissionType::kCamera, false)});
  EXPECT_EQ(1, GetNumberOfInstalledApps());

  InstallApp("appWithMicrophonePermission",
             {std::make_pair(apps::PermissionType::kMicrophone, true)});
  EXPECT_EQ(2, GetNumberOfInstalledApps());

  // Update already installed app.
  UpdateApp("appWithCameraPermission",
            {std::make_pair(apps::PermissionType::kCamera, true)},
            std::nullopt);
  EXPECT_EQ(2, GetNumberOfInstalledApps());

  // Install app which is not relevant for Privacy controls sensor subpages.
  InstallApp("appWithNoRelevantPermission",
             {std::make_pair(apps::PermissionType::kNotifications, true)},
             /*wait_for_update_to_propagate=*/false);
  EXPECT_EQ(2, GetNumberOfInstalledApps());

  InstallApp("appWithLocationPermission",
             {std::make_pair(apps::PermissionType::kLocation, false)});
  EXPECT_EQ(3, GetNumberOfInstalledApps());

  UninstallApp("appWithMicrophonePermission");
  EXPECT_EQ(2, GetNumberOfInstalledApps());
}

TEST_F(AppPermissionHandlerTest, GetSystemAppsThatUseCamera) {
  InstallApp(web_app::kCameraAppId,
             {std::make_pair(apps::PermissionType::kCamera, true)});
  InstallApp(web_app::kPersonalizationAppId,
             {std::make_pair(apps::PermissionType::kCamera, true)});
  InstallApp("systemAppThatDoesNotUseCamera",
             {std::make_pair(apps::PermissionType::kCamera, true)});
  InstallApp(ash::kChromeUIUntrustedProjectorSwaAppId,
             {std::make_pair(apps::PermissionType::kCamera, true)});

  EXPECT_EQ(3, GetNumberOfSystemAppsThatUseCamera());
}

TEST_F(AppPermissionHandlerTest, GetSystemAppsThatUseMicrophone) {
  InstallApp(web_app::kCameraAppId,
             {std::make_pair(apps::PermissionType::kMicrophone, true)});
  InstallApp("systemAppThatDoesNotUseMicrophone",
             {std::make_pair(apps::PermissionType::kMicrophone, true)});
  InstallApp(ash::kChromeUIUntrustedProjectorSwaAppId,
             {std::make_pair(apps::PermissionType::kMicrophone, true)});
  InstallApp(ash::eche_app::kEcheAppId,
             {std::make_pair(apps::PermissionType::kMicrophone, true)});

  EXPECT_EQ(3, GetNumberOfSystemAppsThatUseMicrophone());
}

TEST_F(AppPermissionHandlerTest, OpenCameraBrowserPermissionSettings) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(chrome::kBrowserCameraPermissionsSettingsURL),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab));
  OpenBrowserPermissionSettings(apps::PermissionType::kCamera);
}

TEST_F(AppPermissionHandlerTest, OpenLocationBrowserPermissionSettings) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(chrome::kBrowserLocationPermissionsSettingsURL),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab));
  OpenBrowserPermissionSettings(apps::PermissionType::kLocation);
}

TEST_F(AppPermissionHandlerTest, OpenMicrophoneBrowserPermissionSettings) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(chrome::kBrowserMicrophonePermissionsSettingsURL),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab));
  OpenBrowserPermissionSettings(apps::PermissionType::kMicrophone);
}

}  // namespace ash::settings
