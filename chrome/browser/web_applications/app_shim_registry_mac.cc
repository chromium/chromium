// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_shim_registry_mac.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace {
const char kAppShims[] = "app_shims";
const char kInstalledProfiles[] = "installed_profiles";
const char kLastActiveProfiles[] = "last_active_profiles";
}  // namespace

// static
AppShimRegistry* AppShimRegistry::Get() {
  static base::NoDestructor<AppShimRegistry> instance;
  return instance.get();
}

void AppShimRegistry::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAppShims);
}

std::set<base::FilePath> AppShimRegistry::GetInstalledProfilesForApp(
    const std::string& app_id) const {
  std::set<base::FilePath> installed_profiles;
  GetProfilesSetForApp(app_id, kInstalledProfiles, &installed_profiles);
  return installed_profiles;
}

std::set<base::FilePath> AppShimRegistry::GetLastActiveProfilesForApp(
    const std::string& app_id) const {
  std::set<base::FilePath> last_active_profiles;
  GetProfilesSetForApp(app_id, kLastActiveProfiles, &last_active_profiles);

  // Cull out any profiles that are not installed.
  std::set<base::FilePath> installed_profiles;
  GetProfilesSetForApp(app_id, kInstalledProfiles, &installed_profiles);
  for (auto it = last_active_profiles.begin();
       it != last_active_profiles.end();) {
    if (installed_profiles.count(*it))
      it++;
    else
      last_active_profiles.erase(it++);
  }
  return last_active_profiles;
}

void AppShimRegistry::GetProfilesSetForApp(
    const std::string& app_id,
    const std::string& profiles_key,
    std::set<base::FilePath>* profiles) const {
  const base::Value* cache = GetPrefService()->GetDictionary(kAppShims);
  const base::Value* app_info = cache->FindDictKey(app_id);
  if (!app_info)
    return;
  const base::Value* profile_values = app_info->FindListKey(profiles_key);
  if (!profile_values)
    return;
  for (const auto& profile_path_value : profile_values->GetListDeprecated()) {
    if (profile_path_value.is_string())
      profiles->insert(GetFullProfilePath(profile_path_value.GetString()));
  }
}

void AppShimRegistry::OnAppInstalledForProfile(const std::string& app_id,
                                               const base::FilePath& profile) {
  std::set<base::FilePath> installed_profiles =
      GetInstalledProfilesForApp(app_id);
  if (installed_profiles.count(profile))
    return;
  installed_profiles.insert(profile);
  SetAppInfo(app_id, &installed_profiles, nullptr);
}

bool AppShimRegistry::OnAppUninstalledForProfile(
    const std::string& app_id,
    const base::FilePath& profile) {
  auto installed_profiles = GetInstalledProfilesForApp(app_id);
  auto found = installed_profiles.find(profile);
  if (found != installed_profiles.end()) {
    installed_profiles.erase(profile);
    SetAppInfo(app_id, &installed_profiles, nullptr);
  }
  return installed_profiles.empty();
}

void AppShimRegistry::OnAppQuit(const std::string& app_id,
                                std::set<base::FilePath> last_active_profiles) {
  SetAppInfo(app_id, nullptr, &last_active_profiles);
}

std::set<std::string> AppShimRegistry::GetInstalledAppsForProfile(
    const base::FilePath& profile) const {
  std::set<std::string> result;
  const base::Value* app_shims = GetPrefService()->GetDictionary(kAppShims);
  if (!app_shims)
    return result;
  for (const auto iter_app : app_shims->DictItems()) {
    const base::Value* installed_profiles_list =
        iter_app.second.FindListKey(kInstalledProfiles);
    if (!installed_profiles_list)
      continue;
    for (const auto& profile_path_value :
         installed_profiles_list->GetListDeprecated()) {
      if (!profile_path_value.is_string())
        continue;
      if (profile == GetFullProfilePath(profile_path_value.GetString())) {
        result.insert(iter_app.first);
        break;
      }
    }
  }
  return result;
}

void AppShimRegistry::SetPrefServiceAndUserDataDirForTesting(
    PrefService* pref_service,
    const base::FilePath& user_data_dir) {
  override_pref_service_ = pref_service;
  override_user_data_dir_ = user_data_dir;
}

base::Value AppShimRegistry::AsDebugValue() const {
  const base::Value* app_shims = GetPrefService()->GetDictionary(kAppShims);
  if (!app_shims)
    return base::Value(base::Value::Type::DICTIONARY);

  return app_shims->Clone();
}

PrefService* AppShimRegistry::GetPrefService() const {
  if (override_pref_service_)
    return override_pref_service_;
  return g_browser_process->local_state();
}

base::FilePath AppShimRegistry::GetFullProfilePath(
    const std::string& profile_path) const {
  base::FilePath relative_profile_path(profile_path);
  if (!override_user_data_dir_.empty())
    return override_user_data_dir_.Append(relative_profile_path);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->user_data_dir().Append(relative_profile_path);
}

void AppShimRegistry::SetAppInfo(
    const std::string& app_id,
    const std::set<base::FilePath>* installed_profiles,
    const std::set<base::FilePath>* last_active_profiles) {
  prefs::ScopedDictionaryPrefUpdate update(GetPrefService(), kAppShims);

  // If there are no installed profiles, clear the app's key.
  if (installed_profiles && installed_profiles->empty()) {
    update->Remove(app_id);
    return;
  }

  // Look up dictionary for the app.
  std::unique_ptr<prefs::DictionaryValueUpdate> app_info;
  if (!update->GetDictionaryWithoutPathExpansion(app_id, &app_info)) {
    // If the key for the app doesn't exist, don't add it unless we are
    // specifying a new |installed_profiles| (e.g, for when the app exits
    // during uninstall and tells us its last-used profile after we just
    // removed the entry for the app).
    if (!installed_profiles)
      return;
    app_info = update->SetDictionaryWithoutPathExpansion(
        app_id, std::make_unique<base::DictionaryValue>());
  }
  if (installed_profiles) {
    auto values = std::make_unique<base::ListValue>();
    for (const auto& profile : *installed_profiles)
      values->Append(profile.BaseName().value());
    app_info->Set(kInstalledProfiles, std::move(values));
  }
  if (last_active_profiles) {
    auto values = std::make_unique<base::ListValue>();
    for (const auto& profile : *last_active_profiles)
      values->Append(profile.BaseName().value());
    app_info->Set(kLastActiveProfiles, std::move(values));
  }
}
