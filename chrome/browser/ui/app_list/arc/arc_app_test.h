// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/arc/mojom/app.mojom.h"

namespace arc {
namespace mojom {
class AppInfo;
}
class ArcPlayStoreEnabledPreferenceHandler;
class ArcServiceManager;
class ArcSessionManager;
class FakeAppInstance;
}

namespace chromeos {
class FakeChromeUserManager;
}

namespace user_manager {
class ScopedUserManager;
class User;
}

class ArcAppListPrefs;
class Profile;

// Helper class to initialize arc bridge to work with arc apps in unit tests.
class ArcAppTest {
 public:
  ArcAppTest();
  virtual ~ArcAppTest();

  void SetUp(Profile* profile);
  void TearDown();

  // Public methods to modify AppInstance for unit_tests.
  void StopArcInstance();
  void RestartArcInstance();

  static std::string GetAppId(const arc::mojom::AppInfo& app_info);
  static std::string GetAppId(const arc::mojom::ShortcutInfo& shortcut);

  // Helper that clones packages info array.
  static std::vector<arc::mojom::ArcPackageInfoPtr> ClonePackages(
      const std::vector<arc::mojom::ArcPackageInfoPtr>& packages);

  const std::vector<arc::mojom::ArcPackageInfoPtr>& fake_packages() const {
    return fake_packages_;
  }

  // Adds package info and takes ownership.
  void AddPackage(arc::mojom::ArcPackageInfoPtr package);

  void RemovePackage(const std::string& package_name);

  void WaitForDefaultApps();

  // The 0th item is sticky but not the followings.
  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return fake_apps_;
  }

  const std::vector<arc::mojom::AppInfo>& fake_default_apps() const {
    return fake_default_apps_;
  }

  const std::vector<arc::mojom::ShortcutInfo>& fake_shortcuts() const {
    return fake_shortcuts_;
  }

  chromeos::FakeChromeUserManager* GetUserManager();

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  ArcAppListPrefs* arc_app_list_prefs() { return arc_app_list_pref_; }

  arc::ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }
  arc::ArcServiceManager* arc_service_manager() {
    return arc_service_manager_.get();
  }

  void set_wait_default_apps(bool wait_default_apps) {
    wait_default_apps_ = wait_default_apps;
  }

  void set_activate_arc_on_start(bool activate_arc_on_start) {
    activate_arc_on_start_ = activate_arc_on_start;
  }

 private:
  const user_manager::User* CreateUserAndLogin();
  bool FindPackage(const std::string& package_name);
  void CreateFakeAppsAndPackages();

  // Unowned pointer.
  Profile* profile_ = nullptr;

  ArcAppListPrefs* arc_app_list_pref_ = nullptr;

  bool wait_default_apps_ = true;

  // If set to true ARC would be automatically enabled on test start up.
  bool activate_arc_on_start_ = true;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<arc::ArcPlayStoreEnabledPreferenceHandler>
      arc_play_store_enabled_preference_handler_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::vector<arc::mojom::AppInfo> fake_apps_;
  std::vector<arc::mojom::AppInfo> fake_default_apps_;
  std::vector<arc::mojom::ArcPackageInfoPtr> fake_packages_;
  std::vector<arc::mojom::ShortcutInfo> fake_shortcuts_;

  bool dbus_thread_manager_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppTest);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
