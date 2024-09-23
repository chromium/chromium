// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  return package;
}

arc::mojom::AppInfoPtr CreateArcAppInfo(const std::string& package_name) {
  return arc::mojom::AppInfo::New(package_name, package_name,
                                  base::StrCat({package_name, ".", "activity"}),
                                  true /* sticky */);
}

// Helper class that initializes a run loop and waits for an app update for the
// specified app before quitting the run loop.
class AppUpdateWaiter {
 public:
  AppUpdateWaiter() = default;
  ~AppUpdateWaiter() = default;

  void SetUp(const std::string& app_id) {
    condition_met_ = false;
    app_id_ = app_id;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void Wait() {
    if (!condition_met_) {
      run_loop_->Run();
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

// Implementation of the AppParentalControlsObserver for testing purposes.
class AppParentalControlsTestObserver
    : public app_parental_controls::mojom::AppParentalControlsObserver {
 public:
  AppParentalControlsTestObserver() = default;
  ~AppParentalControlsTestObserver() override = default;

  void OnAppInstalledOrUpdated(
      app_parental_controls::mojom::AppPtr app) override {
    if (app_readiness_changed_.contains(app->id)) {
      ++app_readiness_changed_[app->id];
    } else {
      app_readiness_changed_[app->id] = 1;
    }
    waiter_.MaybeStop(app->id);
    recently_updated_app_ = std::move(app);
  }

  void OnAppUninstalled(app_parental_controls::mojom::AppPtr app) override {
    waiter_.MaybeStop(app->id);
    recently_uninstalled_app_ = std::move(app);
  }

  mojo::PendingRemote<app_parental_controls::mojom::AppParentalControlsObserver>
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

  const app_parental_controls::mojom::AppPtr& recently_updated_app() const {
    return recently_updated_app_;
  }

  const app_parental_controls::mojom::AppPtr& recently_uninstalled_app() const {
    return recently_uninstalled_app_;
  }

  int GetReadinessChangeCount(const std::string& app_id) const {
    return app_readiness_changed_.contains(app_id)
               ? app_readiness_changed_.at(app_id)
               : 0;
  }

 private:
  app_parental_controls::mojom::AppPtr recently_updated_app_;
  app_parental_controls::mojom::AppPtr recently_uninstalled_app_;
  std::map<std::string, int> app_readiness_changed_;
  AppUpdateWaiter waiter_;

  mojo::Receiver<app_parental_controls::mojom::AppParentalControlsObserver>
      receiver_{this};
};
}  // namespace

class AppParentalControlsHandlerBrowserTest : public InProcessBrowserTest {
 protected:
  AppParentalControlsHandlerBrowserTest() = default;
  ~AppParentalControlsHandlerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    arc_app_list_prefs_ = ArcAppListPrefs::Get(profile());
    EXPECT_TRUE(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    arc_app_instance_ =
        std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);

    observer_ = std::make_unique<AppParentalControlsTestObserver>();
  }

  void TearDownOnMainThread() override {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        arc_app_instance_.get());
    arc_app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
    arc_app_list_prefs_ = nullptr;
  }

  std::string InstallArcApp(const std::string& package_name) {
    base::RunLoop run_loop;
    arc_app_instance_->SendPackageAdded(
        CreateArcAppPackage(package_name)->Clone());

    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.emplace_back(CreateArcAppInfo(package_name));

    arc_app_instance_->SendPackageAppListRefreshed(package_name, apps);
    run_loop.RunUntilIdle();

    return arc::ArcPackageNameToAppId(package_name, profile());
  }

  void UninstallArcPackage(const std::string& package_name) {
    arc_app_instance_->UninstallPackage(package_name);
  }

  void InstallNonArcApps() {
    auto web_app_install_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL("https://example.org"));
    webapps::AppId app_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_install_info));
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
  }

  AppParentalControlsHandler* GetHandler() {
    return OsSettingsManagerFactory::GetForProfile(profile())
        ->app_parental_controls_handler();
  }

  Profile* profile() { return browser()->profile(); }
  AppParentalControlsTestObserver* observer() { return observer_.get(); }

 private:
  raw_ptr<ArcAppListPrefs> arc_app_list_prefs_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> arc_app_instance_;
  std::unique_ptr<AppParentalControlsTestObserver> observer_;
};

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       OnlyManageableArcAppsFetched) {
  AppParentalControlsHandler* handler = GetHandler();
  InstallNonArcApps();
  std::string arc_app_id1 = InstallArcApp("com.example.app1");
  std::string arc_app_id2 = InstallArcApp("com.example.app2");

  base::RunLoop run_loop;
  handler->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 2u);
        EXPECT_EQ(apps[0]->id, arc_app_id2);
        EXPECT_EQ(apps[1]->id, arc_app_id1);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       BlockAndUnblockApp) {
  AppParentalControlsHandler* handler = GetHandler();
  handler->AddObserver(observer()->GenerateRemote());

  std::string arc_app_id = InstallArcApp("com.example.app1");

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 1);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  observer()->SetUpWaiterForAppUpdate(arc_app_id);
  handler->UpdateApp(arc_app_id, /*is_blocked=*/true);
  observer()->WaitForAppUpdate();

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 2);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  base::RunLoop run_loop;
  handler->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, arc_app_id);
        EXPECT_EQ(apps[0]->is_blocked, true);
        run_loop.Quit();
      }));
  run_loop.Run();

  observer()->SetUpWaiterForAppUpdate(arc_app_id);
  handler->UpdateApp(arc_app_id, /*is_blocked=*/false);
  observer()->WaitForAppUpdate();

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 3);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  base::RunLoop run_loop2;
  handler->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, arc_app_id);
        EXPECT_EQ(apps[0]->is_blocked, false);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest, UninstallApp) {
  AppParentalControlsHandler* handler = GetHandler();
  handler->AddObserver(observer()->GenerateRemote());

  std::string arc_app_package1 = "com.example.app1";
  std::string arc_app_id1 = InstallArcApp(arc_app_package1);
  std::string arc_app_id2 = InstallArcApp("com.example.app2");

  base::RunLoop run_loop;
  handler->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 2u);
        run_loop.Quit();
      }));
  run_loop.Run();

  observer()->SetUpWaiterForAppUpdate(arc_app_id1);
  UninstallArcPackage(arc_app_package1);
  observer()->WaitForAppUpdate();

  base::RunLoop run_loop2;
  handler->GetApps(base::BindLambdaForTesting(
      [&](std::vector<app_parental_controls::mojom::AppPtr> apps) -> void {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->id, arc_app_id2);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  EXPECT_EQ(observer()->recently_uninstalled_app()->id, arc_app_id1);
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       OnControlsDisabled) {
  AppParentalControlsHandler* handler = GetHandler();
  handler->AddObserver(observer()->GenerateRemote());

  std::string arc_app_id = InstallArcApp("com.example.app1");

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 1);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  std::string pin = "123456";
  base::RunLoop run_loop;
  handler->SetUpPin(
      pin, base::BindLambdaForTesting(([&](bool isSuccess) -> void {
        ASSERT_TRUE(isSuccess);
        ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
            prefs::kOnDeviceAppControlsSetupCompleted));
        EXPECT_EQ(
            profile()->GetPrefs()->GetString(prefs::kOnDeviceAppControlsPin),
            pin);
        run_loop.Quit();
      })));

  observer()->SetUpWaiterForAppUpdate(arc_app_id);
  handler->UpdateApp(arc_app_id, /*is_blocked=*/true);
  observer()->WaitForAppUpdate();

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 2);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  observer()->SetUpWaiterForAppUpdate(arc_app_id);
  handler->OnControlsDisabled();
  observer()->WaitForAppUpdate();

  EXPECT_EQ(observer()->GetReadinessChangeCount(arc_app_id), 3);
  EXPECT_EQ(observer()->recently_updated_app()->id, arc_app_id);

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kOnDeviceAppControlsSetupCompleted));
  EXPECT_EQ(profile()->GetPrefs()->GetString(prefs::kOnDeviceAppControlsPin),
            std::string());
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinSuccess) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      "123456",
      base::BindLambdaForTesting(
          [&](app_parental_controls::mojom::PinValidationResult result)
              -> void {
            EXPECT_EQ(result, app_parental_controls::mojom::
                                  PinValidationResult::kPinValidationSuccess);
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinEmptyString) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      std::string(),
      base::BindLambdaForTesting([&](app_parental_controls::mojom::
                                         PinValidationResult result) -> void {
        EXPECT_EQ(
            result,
            app_parental_controls::mojom::PinValidationResult::kPinLengthError);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinTooShortError) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      "1234",
      base::BindLambdaForTesting([&](app_parental_controls::mojom::
                                         PinValidationResult result) -> void {
        EXPECT_EQ(
            result,
            app_parental_controls::mojom::PinValidationResult::kPinLengthError);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinTooLongError) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      "1234567",
      base::BindLambdaForTesting([&](app_parental_controls::mojom::
                                         PinValidationResult result) -> void {
        EXPECT_EQ(
            result,
            app_parental_controls::mojom::PinValidationResult::kPinLengthError);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinNumericError) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      "1a3%56",
      base::BindLambdaForTesting(
          [&](app_parental_controls::mojom::PinValidationResult result)
              -> void {
            EXPECT_EQ(result, app_parental_controls::mojom::
                                  PinValidationResult::kPinNumericError);
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest,
                       ValidatePinWithSpecialCharactersNumericError) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->ValidatePin(
      "1a3%û", base::BindLambdaForTesting(
                   [&](app_parental_controls::mojom::PinValidationResult result)
                       -> void {
                     EXPECT_EQ(result,
                               app_parental_controls::mojom::
                                   PinValidationResult::kPinNumericError);
                     run_loop.Quit();
                   }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest, SetUpValidPin) {
  AppParentalControlsHandler* handler = GetHandler();

  std::string pin = "123456";
  base::RunLoop run_loop;
  handler->SetUpPin(
      pin, base::BindLambdaForTesting([&](bool is_success) -> void {
        ASSERT_TRUE(is_success);
        ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
            prefs::kOnDeviceAppControlsSetupCompleted));
        EXPECT_EQ(
            profile()->GetPrefs()->GetString(prefs::kOnDeviceAppControlsPin),
            pin);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AppParentalControlsHandlerBrowserTest, SetUpInvalidPin) {
  AppParentalControlsHandler* handler = GetHandler();

  base::RunLoop run_loop;
  handler->SetUpPin("1a3%56",
                    base::BindLambdaForTesting([&](bool is_success) -> void {
                      ASSERT_FALSE(is_success);
                      run_loop.Quit();
                    }));
  run_loop.Run();
}

}  // namespace ash::settings
