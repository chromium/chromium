// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_test.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName1[] = "fake.package.name1";
constexpr char kPackageName2[] = "fake.package.name2";
constexpr char kPackageName3[] = "fake.package.name3";
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::ShortcutInfo& shortcut) {
  return ArcAppListPrefs::GetAppId(shortcut.package_name, shortcut.intent_uri);
}

// static
std::vector<arc::mojom::ArcPackageInfoPtr> ArcAppTest::ClonePackages(
    const std::vector<arc::mojom::ArcPackageInfoPtr>& packages) {
  std::vector<arc::mojom::ArcPackageInfoPtr> result;
  for (const auto& package : packages)
    result.emplace_back(package->Clone());
  return result;
}

ArcAppTest::ArcAppTest() {
  user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<chromeos::FakeChromeUserManager>());
  CreateFakeAppsAndPackages();
}

ArcAppTest::~ArcAppTest() {
}

chromeos::FakeChromeUserManager* ArcAppTest::GetUserManager() {
  return static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
}

void ArcAppTest::SetUp(Profile* profile) {
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Initialize();
    dbus_thread_manager_initialized_ = true;
  }
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  DCHECK(!profile_);
  profile_ = profile;

  arc::ResetArcAllowedCheckForTesting(profile_);

  const user_manager::User* user = CreateUserAndLogin();

  // If for any reason the garbage collector kicks in while we are waiting for
  // an icon, have the user-to-profile mapping ready to avoid using the real
  // profile manager (which is null).
  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                    profile_);

  // A valid |arc_app_list_prefs_| is needed for the ARC bridge service and the
  // ARC auth service.
  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
  if (!arc_app_list_pref_) {
    ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
        profile_);
  }
  arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
  arc_session_manager_ = std::make_unique<arc::ArcSessionManager>(
      std::make_unique<arc::ArcSessionRunner>(
          base::BindRepeating(arc::FakeArcSession::Create)));
  DCHECK(arc::ArcSessionManager::Get());
  arc::ArcSessionManager::SetUiEnabledForTesting(false);
  arc_session_manager_->SetProfile(profile_);
  arc_session_manager_->Initialize();
  arc_play_store_enabled_preference_handler_ =
      std::make_unique<arc::ArcPlayStoreEnabledPreferenceHandler>(
          profile_, arc_session_manager_.get());
  arc_play_store_enabled_preference_handler_->Start();

  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_app_list_pref_);
  if (wait_default_apps_)
    WaitForDefaultApps();

  // Check initial conditions.
  if (activate_arc_on_start_) {
    if (!arc::ShouldArcAlwaysStart())
      arc::SetArcPlayStoreEnabledForProfile(profile_, true);
    if (!arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_))
      EXPECT_TRUE(arc_session_manager_->enable_requested());

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());

    // TODO(khmel): Resolve this gracefully. Set of default app tests does not
    // expect waiting in ArcAppTest setup.
    if (wait_default_apps_)
      WaitForInstanceReady(arc_service_manager_->arc_bridge_service()->app());
  }

  // Ensure that the singleton apps::ArcApps is constructed.
  apps::ArcAppsFactory::GetForProfile(profile_);
}

void ArcAppTest::WaitForDefaultApps() {
  DCHECK(arc_app_list_pref_);
  base::RunLoop run_loop;
  arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
  run_loop.Run();
}

void ArcAppTest::CreateFakeAppsAndPackages() {
  arc::mojom::AppInfo app;
  // Make sure we have enough data for test.
  for (int i = 0; i < 3; ++i) {
    app.name = base::StringPrintf("Fake App %d", i);
    app.package_name = base::StringPrintf("fake.app.%d", i);
    app.activity = base::StringPrintf("fake.app.%d.activity", i);
    app.sticky = false;
    fake_apps_.push_back(app);
  }
  fake_apps_[0].sticky = true;

  app.name = "TestApp1";
  app.package_name = "test.app1";
  app.activity = "test.app1.activity";
  app.sticky = true;
  fake_default_apps_.push_back(app);

  app.name = "TestApp2";
  app.package_name = "test.app2";
  app.activity = "test.app2.activity";
  app.sticky = true;
  fake_default_apps_.push_back(app);

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions1;
  permissions1.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName1 /* package_name */, 1 /* package_version */,
      1 /* last_backup_android_id */, 1 /* last_backup_time */,
      false /* sync */, false /* system */, false /* vpn_provider */,
      nullptr /* web_app_info */, base::nullopt, std::move(permissions1)));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions2;
  permissions2.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions2.emplace(arc::mojom::AppPermission::MICROPHONE,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName2 /* package_name */, 2 /* package_version */,
      2 /* last_backup_android_id */, 2 /* last_backup_time */, true /* sync */,
      false /* system */, false /* vpn_provider */, nullptr /* web_app_info */,
      base::nullopt, std::move(permissions2)));

  base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
      permissions3;
  permissions3.emplace(arc::mojom::AppPermission::CAMERA,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions3.emplace(arc::mojom::AppPermission::MICROPHONE,
                       arc::mojom::PermissionState::New(false /* granted */,
                                                        false /* managed */));
  permissions3.emplace(arc::mojom::AppPermission::LOCATION,
                       arc::mojom::PermissionState::New(true /* granted */,
                                                        false /* managed */));
  fake_packages_.emplace_back(arc::mojom::ArcPackageInfo::New(
      kPackageName3 /* package_name */, 3 /* package_version */,
      3 /* last_backup_android_id */, 3 /* last_backup_time */,
      false /* sync */, false /* system */, false /* vpn_provider */,
      nullptr /* web_app_info */, base::nullopt, std::move(permissions3)));

  for (int i = 0; i < 3; ++i) {
    arc::mojom::ShortcutInfo shortcut_info;
    shortcut_info.name = base::StringPrintf("Fake Shortcut %d", i);
    shortcut_info.package_name = base::StringPrintf("fake.shortcut.%d", i);
    shortcut_info.intent_uri =
        base::StringPrintf("#Intent;fake.shortcut.%d.intent_uri", i);
    shortcut_info.icon_resource_id =
        base::StringPrintf("fake.shortcut.%d.icon_resource_id", i);
    fake_shortcuts_.push_back(shortcut_info);
  }
}

void ArcAppTest::TearDown() {
  app_instance_.reset();
  arc_play_store_enabled_preference_handler_.reset();
  arc_session_manager_.reset();
  arc_service_manager_.reset();
  arc::ResetArcAllowedCheckForTesting(profile_);
  if (dbus_thread_manager_initialized_) {
    // DBusThreadManager may be initialized from other testing utility,
    // such as ash::AshTestHelper::SetUp(), so Shutdown() only when
    // it is initialized in ArcAppTest::SetUp().
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }
  profile_ = nullptr;
}

void ArcAppTest::StopArcInstance() {
  arc_service_manager_->arc_bridge_service()->app()->CloseInstance(
      app_instance_.get());
}

void ArcAppTest::RestartArcInstance() {
  auto* bridge_service = arc_service_manager_->arc_bridge_service();
  bridge_service->app()->CloseInstance(app_instance_.get());
  app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
  bridge_service->app()->SetInstance(app_instance_.get());
  WaitForInstanceReady(bridge_service->app());
}

const user_manager::User* ArcAppTest::CreateUserAndLogin() {
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile_->GetProfileUserName(), "1234567890"));
  const user_manager::User* user = GetUserManager()->AddUser(account_id);
  GetUserManager()->LoginUser(account_id);
  return user;
}

void ArcAppTest::AddPackage(arc::mojom::ArcPackageInfoPtr package) {
  if (FindPackage(package->package_name))
    return;
  fake_packages_.push_back(std::move(package));
}

void ArcAppTest::RemovePackage(const std::string& package_name) {
  base::EraseIf(fake_packages_, [package_name](const auto& package) {
    return package->package_name == package_name;
  });
}

bool ArcAppTest::FindPackage(const std::string& package_name) {
  return std::find_if(fake_packages_.begin(), fake_packages_.end(),
                      [package_name](const auto& package) {
                        return package->package_name == package_name;
                      }) != fake_packages_.end();
}
