// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_REGISTRY_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_REGISTRY_MAC_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"

class PrefService;
class PrefRegistrySimple;

// This class is used to store information about which app shims have been
// installed for which profiles in local storage. This is used to:
//  - Open the last active profile when an app shim is launched.
//  - Populate the profile switcher menu in the app with only those profile
//    for which the app is installed.
//  - Only delete the app shim when it has been uninstalled for all profiles.
// All base::FilePath arguments to functions are expected to be full profile
// paths (e.g, the result of calling Profile::GetPath).
//
// This class should arguably be a extensions::ExtensionRegistryObserver. It
// is not (and instead is called by WebAppShortcutManager, which is one),
// because apps are in the process of being disentangled from extensions.
class AppShimRegistry {
 public:
  AppShimRegistry(const AppShimRegistry& other) = delete;
  AppShimRegistry& operator=(const AppShimRegistry& other) = delete;

  static AppShimRegistry* Get();
  void RegisterLocalPrefs(PrefRegistrySimple* registry);

  // Query the profiles paths for which the specified app is installed.
  std::set<base::FilePath> GetInstalledProfilesForApp(
      const std::string& app_id) const;

  // Query the profiles paths that were last open in the app (which are the
  // profiles to open when the app starts).
  std::set<base::FilePath> GetLastActiveProfilesForApp(
      const std::string& app_id) const;

  // Called when an app is installed for a profile (or any other action that
  // would create an app shim, e.g: launching the shim).
  void OnAppInstalledForProfile(const std::string& app_id,
                                const base::FilePath& profile);

  // Called when an app is uninstalled for a profile. Returns true if no
  // profiles have this app installed anymore.
  bool OnAppUninstalledForProfile(const std::string& app_id,
                                  const base::FilePath& profile);

  // Called when an app quits, providing a list of the profiles that were
  // in use at the time of quitting.
  void OnAppQuit(const std::string& app_id,
                 std::set<base::FilePath> active_profiles);

  // Return all apps installed for the specified profile. Used to delete apps
  // when a profile is removed.
  std::set<std::string> GetInstalledAppsForProfile(
      const base::FilePath& profile) const;

  // Helper functions for testing.
  void SetPrefServiceAndUserDataDirForTesting(
      PrefService* pref_service,
      const base::FilePath& user_data_dir);

  // For logging and debug purposes.
  base::Value AsDebugValue() const;

 protected:
  friend class base::NoDestructor<AppShimRegistry>;

  AppShimRegistry() = default;
  ~AppShimRegistry() = default;

  PrefService* GetPrefService() const;
  base::FilePath GetFullProfilePath(const std::string& profile_path) const;

  // Helper function used by GetInstalledProfilesForApp and
  // GetLastActiveProfilesForApp.
  void GetProfilesSetForApp(const std::string& app_id,
                            const std::string& profiles_key,
                            std::set<base::FilePath>* profiles) const;

  // Update the local storage for |app_id|. Update |installed_profiles| and
  // |last_active_profiles| only if they are non-nullptr. If
  // |installed_profiles| is non-nullptr and empty, remove the entry for
  // |app_id|.
  void SetAppInfo(const std::string& app_id,
                  const std::set<base::FilePath>* installed_profiles,
                  const std::set<base::FilePath>* last_active_profiles);

  raw_ptr<PrefService> override_pref_service_ = nullptr;
  base::FilePath override_user_data_dir_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_REGISTRY_MAC_H_
