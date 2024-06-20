// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_REGISTRY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_REGISTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

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

  // Returns true if `profile` is among the profile paths in which the specified
  // app is installed.
  bool IsAppInstalledInProfile(const std::string& app_id,
                               const base::FilePath& profile) const;

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

  // Called to save a list of the profiles that were last in use for an app.
  // This is called for example when an app quits, providing the profiles that
  // were in use at that time.
  void SaveLastActiveProfilesForApp(const std::string& app_id,
                                    std::set<base::FilePath> active_profiles);

  // Return all apps installed for the specified profile. Used to delete apps
  // when a profile is removed.
  std::set<std::string> GetInstalledAppsForProfile(
      const base::FilePath& profile) const;

  // Returns all apps installed in multiple profiles. Used for metrics.
  std::set<std::string> GetAppsInstalledInMultipleProfiles() const;

  // Called when the file and/or protocol handlers for an app are updated in a
  // specific profile. Used to calculate the union of all handlers for a app
  // when updating the app shim.
  void SaveFileHandlersForAppAndProfile(
      const std::string& app_id,
      const base::FilePath& profile,
      std::set<std::string> file_handler_extensions,
      std::set<std::string> file_handler_mime_types);
  void SaveProtocolHandlersForAppAndProfile(
      const std::string& app_id,
      const base::FilePath& profile,
      std::set<std::string> protocol_handlers);

  struct HandlerInfo {
    HandlerInfo();
    ~HandlerInfo();
    HandlerInfo(HandlerInfo&&);
    HandlerInfo(const HandlerInfo&);
    HandlerInfo& operator=(HandlerInfo&&);
    HandlerInfo& operator=(const HandlerInfo&);

    bool IsEmpty() const {
      return file_handler_extensions.empty() &&
             file_handler_mime_types.empty() && protocol_handlers.empty();
    }

    std::set<std::string> file_handler_extensions;
    std::set<std::string> file_handler_mime_types;
    std::set<std::string> protocol_handlers;
  };

  // Returns all the file and protocol handlers for the given app, keyed by
  // profile path.
  std::map<base::FilePath, HandlerInfo> GetHandlersForApp(
      const std::string& app_id);

  // Return whether a code directory hash has ever been associated with any app.
  bool HasSavedAnyCdHashes() const;

  // Associate the given code directory hash with a given app.
  void SaveCdHashForApp(const std::string& app_id,
                        base::span<const uint8_t> cd_hash);

  // Verify that the given code directory hash matches the one previously
  // associated with the given app.
  bool VerifyCdHashForApp(const std::string& app_id,
                          base::span<const uint8_t> cd_hash);

  // Called when changes to the system level notification permission status for
  // the given app have been detected.
  void SaveNotificationPermissionStatusForApp(
      const std::string& app_id,
      mac_notifications::mojom::PermissionStatus status);

  // Gets the last known system level notification permission status for the
  // given app. Returns kNotDetermined if no value has been stored.
  mac_notifications::mojom::PermissionStatus
  GetNotificationPermissionStatusForApp(const std::string& app_id);

  // Register a callback to be called any time data associated with an app
  // changes. The callback is passed the app_id of the changed app.
  base::CallbackListSubscription RegisterAppChangedCallback(
      base::RepeatingCallback<void(const std::string&)> callback);

  // Helper functions for testing.
  void SetPrefServiceAndUserDataDirForTesting(
      PrefService* pref_service,
      const base::FilePath& user_data_dir);

  // For logging and debug purposes.
  base::Value::Dict AsDebugDict() const;

 protected:
  friend class base::NoDestructor<AppShimRegistry>;

  AppShimRegistry();
  ~AppShimRegistry();

  PrefService* GetPrefService() const;
  base::FilePath GetFullProfilePath(const std::string& profile_path) const;

  // Helper function used by GetInstalledProfilesForApp and
  // GetLastActiveProfilesForApp.
  void GetProfilesSetForApp(const std::string& app_id,
                            const std::string& profiles_key,
                            std::set<base::FilePath>* profiles) const;

  using HmacKey = std::vector<uint8_t>;
  static constexpr size_t kHmacKeySize = 32;

  // Retrieve the key used to create HMACs of app's code directory hashes,
  // generating a new key if needed.
  HmacKey GetCdHashHmacKey();

  // Helper function used by GetCdHashHmacKey
  // Retrieve the existing key used to create HMACs of app's code directory
  // hashes. Returns nullopt if no key was found or the existing key could not
  // be decoded or decrypted.
  std::optional<HmacKey> GetExistingCdHashHmacKey();

  // Helper function used by GetCdHashHmacKey
  // Encode and encrypt the given HMAC key and save it to preferences.
  void SaveCdHashHmacKey(const HmacKey& key);

  // Update the local storage for |app_id|. Update |installed_profiles| and
  // |last_active_profiles| only if they are non-nullptr. If
  // |installed_profiles| is non-nullptr and empty, remove the entry for
  // |app_id|.
  void SetAppInfo(const std::string& app_id,
                  const std::set<base::FilePath>* installed_profiles,
                  const std::set<base::FilePath>* last_active_profiles,
                  const std::map<base::FilePath, HandlerInfo>* handlers,
                  const std::string* cd_hash_hmac_base64,
                  const mac_notifications::mojom::PermissionStatus*
                      notification_permission_status);

  raw_ptr<PrefService> override_pref_service_ = nullptr;
  base::FilePath override_user_data_dir_;

  base::RepeatingCallbackList<void(const std::string& app_id)>
      app_changed_callbacks_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_REGISTRY_H_
