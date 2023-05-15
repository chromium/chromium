// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/web_applications/externally_installed_prefs_migration_metrics.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// The stored preferences look like:
//
// "web_apps": {
//   "extension_ids": {
//     "https://events.google.com/io2016/?utm_source=web_app_manifest": {
//       "extension_id": "mjgafbdfajpigcjmkgmeokfbodbcfijl",
//       "install_source": 1,
//       "is_placeholder": true,
//     },
//     "https://www.chromestatus.com/features": {
//       "extension_id": "fedbieoalmbobgfjapopkghdmhgncnaa",
//       "install_source": 1
//     }
//   }
// }
//
// From the top, prefs::kWebAppsExtensionIDs is "web_apps.extension_ids".
//
// Two levels in is a dictionary (key/value pairs) whose keys are URLs and
// values are leaf dictionaries. Those leaf dictionaries have keys such as
// kExtensionId and kInstallSource.
// The name "extension_id" comes from when PWAs were only backed by the
// Extension system rather than their own. It cannot be changed now that it
// lives persistently in users' profiles.
constexpr char kExtensionId[] = "extension_id";
constexpr char kInstallSource[] = "install_source";
constexpr char kIsPlaceholder[] = "is_placeholder";

constexpr char kAppIdDeleted[] =
    "WebApp.ExternalPrefs.CorruptionFixedRemovedAppId";
constexpr char kInstallUrlsDeleted[] =
    "WebApp.ExternalPrefs.CorruptionFixedInstallUrlsDeleted";

// Returns the base::Value in `pref_service` corresponding to our stored dict
// for `app_id`, or `nullptr` if it doesn't exist.
const base::Value::Dict* GetPreferenceValue(const PrefService* pref_service,
                                            const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& urls_to_dicts =
      pref_service->GetDict(prefs::kWebAppsExtensionIDs);
  // Do a simple O(N) scan for app_id being a value in each dictionary's
  // key/value pairs. We expect both N and the number of times
  // GetPreferenceValue is called to be relatively small in practice. If they
  // turn out to be large, we can write a more sophisticated implementation.
  for (const auto it : urls_to_dicts) {
    if (!it.second.is_dict()) {
      continue;
    }

    const base::Value::Dict& dict = it.second.GetDict();
    const std::string* extension_id = dict.FindString(kExtensionId);
    if (extension_id && (*extension_id == app_id)) {
      return &it.second.GetDict();
    }
  }

  return nullptr;
}

// Correct any corruption that has occurred due to Lacros processes starting in
// inconsistent ways. https://crbug.com/1359205.
void FixMigrationCorruptionFromLacrosSwitch(PrefService* pref_service,
                                            const WebAppRegistrar& registrar) {
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  int install_urls_fixed = 0;
  int app_ids_removed = 0;
  for (const AppId& app_id : registrar.GetAppIds()) {
    if (!registrar.IsInstalledByDefaultManagement(app_id)) {
      continue;
    }
    const WebApp* web_app = registrar.GetAppById(app_id);
    DCHECK(web_app);
    auto default_config_it = web_app->management_to_external_config_map().find(
        WebAppManagement::kDefault);
    // If there is no external config or the install urls are empty, just treat
    // it as a valid install. The preinstall manager should add to the install
    // urls when it synchronizes.
    if (default_config_it ==
            web_app->management_to_external_config_map().end() ||
        default_config_it->second.install_urls.empty()) {
      if (preinstalled_prefs.RemoveByAppId(app_id))
        ++app_ids_removed;
      continue;
    }
    // Remove all install urls that are legitimately installed. If they are all
    // removed, then the pref is removed entirely.
    for (const GURL& install_url : default_config_it->second.install_urls) {
      if (preinstalled_prefs.RemoveByInstallUrl(app_id, install_url))
        ++install_urls_fixed;
    }
  }
  base::UmaHistogramCounts100(kAppIdDeleted, app_ids_removed);
  base::UmaHistogramCounts100(kInstallUrlsDeleted, install_urls_fixed);
}

}  // namespace

// static
void ExternallyInstalledWebAppPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppsExtensionIDs);
}

// static
bool ExternallyInstalledWebAppPrefs::HasAppId(const PrefService* pref_service,
                                              const AppId& app_id) {
  return GetPreferenceValue(pref_service, app_id) != nullptr;
}

// static
bool ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
    const PrefService* pref_service,
    const AppId& app_id,
    ExternalInstallSource install_source) {
  const base::Value::Dict* dict = GetPreferenceValue(pref_service, app_id);
  if (dict == nullptr) {
    return false;
  }

  absl::optional<int> v = dict->FindInt(kInstallSource);
  return (v.has_value() && v.value() == static_cast<int>(install_source));
}

// static
base::flat_map<AppId, base::flat_set<GURL>>
ExternallyInstalledWebAppPrefs::BuildAppIdsMap(
    const PrefService* pref_service,
    ExternalInstallSource install_source) {
  const base::Value::Dict& urls_to_dicts =
      pref_service->GetDict(prefs::kWebAppsExtensionIDs);

  base::flat_map<AppId, base::flat_set<GURL>> ids_to_urls;

  for (auto it : urls_to_dicts) {
    const base::Value* const v = &it.second;
    if (!v->is_dict()) {
      continue;
    }

    const base::Value::Dict& dict = v->GetDict();
    const absl::optional<int> install_source_pref =
        dict.FindInt(kInstallSource);
    if (!install_source_pref ||
        (install_source_pref != static_cast<int>(install_source))) {
      continue;
    }

    const std::string* extension_id = dict.FindString(kExtensionId);
    if (!extension_id) {
      continue;
    }

    GURL url(it.first);
    DCHECK(url.is_valid() && !url.is_empty());
    ids_to_urls[*extension_id] = {url};
  }

  return ids_to_urls;
}

ExternallyInstalledWebAppPrefs::ExternallyInstalledWebAppPrefs(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void ExternallyInstalledWebAppPrefs::Insert(
    const GURL& url,
    const AppId& app_id,
    ExternalInstallSource install_source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedDictPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  update->Set(url.spec(),
              base::Value::Dict()
                  .Set(kExtensionId, app_id)
                  .Set(kInstallSource, static_cast<int>(install_source)));
}

bool ExternallyInstalledWebAppPrefs::Remove(const GURL& url) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  return update->Remove(url.spec());
}

absl::optional<AppId> ExternallyInstalledWebAppPrefs::LookupAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict* dict =
      pref_service_->GetDict(prefs::kWebAppsExtensionIDs).FindDict(url.spec());
  if (!dict) {
    return absl::nullopt;
  }

  const std::string* extension_id = dict->FindString(kExtensionId);
  return extension_id ? absl::make_optional(*extension_id) : absl::nullopt;
}

absl::optional<AppId> ExternallyInstalledWebAppPrefs::LookupPlaceholderAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict* entry =
      pref_service_->GetDict(prefs::kWebAppsExtensionIDs).FindDict(url.spec());
  if (!entry)
    return absl::nullopt;

  absl::optional<bool> is_placeholder = entry->FindBool(kIsPlaceholder);
  if (!is_placeholder.has_value() || !is_placeholder.value())
    return absl::nullopt;

  return *entry->FindString(kExtensionId);
}

void ExternallyInstalledWebAppPrefs::SetIsPlaceholder(const GURL& url,
                                                      bool is_placeholder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(pref_service_->GetDict(prefs::kWebAppsExtensionIDs).Find(url.spec()));
  ScopedDictPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  base::Value::Dict& map = update.Get();

  auto* app_entry = map.FindDict(url.spec());
  DCHECK(app_entry);

  app_entry->Set(kIsPlaceholder, is_placeholder);
}

bool ExternallyInstalledWebAppPrefs::IsPlaceholderApp(
    const AppId& app_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict* app_prefs =
      GetPreferenceValue(pref_service_, app_id);
  if (!app_prefs) {
    return false;
  }
  return app_prefs->FindBool(kIsPlaceholder).value_or(false);
}

// static
ExternallyInstalledWebAppPrefs::ParsedPrefs
ExternallyInstalledWebAppPrefs::ParseExternalPrefsToWebAppData(
    PrefService* pref_service) {
  const base::Value::Dict& urls_to_dicts =
      pref_service->GetDict(prefs::kWebAppsExtensionIDs);
  ParsedPrefs ids_to_parsed_data;

  for (auto it : urls_to_dicts) {
    if (!it.second.is_dict()) {
      continue;
    }

    const base::Value::Dict& dict = it.second.GetDict();
    const std::string* app_id = dict.FindString(kExtensionId);
    if (!app_id) {
      continue;
    }

    const absl::optional<int> source = dict.FindInt(kInstallSource);
    if (!source) {
      continue;
    }

    WebAppManagement::Type source_type = ConvertExternalInstallSourceToSource(
        static_cast<ExternalInstallSource>(source.value()));
    WebApp::ExternalManagementConfig& config =
        ids_to_parsed_data[*app_id][source_type];
    config.is_placeholder = dict.FindBool(kIsPlaceholder).value_or(false);
    config.install_urls.emplace(GURL(it.first));
  }

  return ids_to_parsed_data;
}

// static
void ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
    PrefService* pref_service,
    WebAppSyncBridge* sync_bridge) {
  ExternallyInstalledWebAppPrefs::ParsedPrefs pref_to_app_data =
      ParseExternalPrefsToWebAppData(pref_service);

  const WebAppRegistrar& registrar = sync_bridge->registrar();

  LogDataMetrics(pref_to_app_data.size() != 0,
                 registrar.AppsExistWithExternalConfigData());

  // First migrate data to UserUninstalledPreinstalledWebAppPrefs.
  MigrateExternalPrefDataToPreinstalledPrefs(pref_service, &registrar,
                                             pref_to_app_data);
  ScopedRegistryUpdate update(sync_bridge);
  for (auto it : pref_to_app_data) {
    const WebApp* web_app = registrar.GetAppById(it.first);
    if (web_app) {
      // Sync data across externally installed prefs and web_app DB.
      for (auto parsed_info : it.second) {
        WebAppManagement::Type& source = parsed_info.first;
        if (!web_app->GetSources().test(source))
          continue;

        const WebApp::ExternalConfigMap& config_map =
            web_app->management_to_external_config_map();
        auto map_it = config_map.find(source);
        // Placeholder migration and metrics logging.
        if (map_it != config_map.end() &&
            map_it->second.is_placeholder ==
                parsed_info.second.is_placeholder) {
          LogPlaceholderMigrationState(
              PlaceholderMigrationState::kPlaceholderInfoAlreadyInSync);
        } else {
          WebApp* updated_app = update->UpdateApp(it.first);
          updated_app->AddPlaceholderInfoToManagementExternalConfigMap(
              source, parsed_info.second.is_placeholder);
          LogPlaceholderMigrationState(
              PlaceholderMigrationState::kPlaceholderInfoMigrated);
        }

        // Install URL migration and metrics logging.
        for (auto url : parsed_info.second.install_urls) {
          DCHECK(url.is_valid());
          if (map_it != config_map.end() &&
              base::Contains(map_it->second.install_urls, url)) {
            LogInstallURLMigrationState(
                InstallURLMigrationState::kInstallURLAlreadyInSync);
          } else {
            WebApp* updated_app = update->UpdateApp(it.first);
            updated_app->AddInstallURLToManagementExternalConfigMap(
                parsed_info.first, url);
            LogInstallURLMigrationState(
                InstallURLMigrationState::kInstallURLMigrated);
          }
        }
      }
    }
  }

  FixMigrationCorruptionFromLacrosSwitch(pref_service, registrar);
}

// static
void ExternallyInstalledWebAppPrefs::MigrateExternalPrefDataToPreinstalledPrefs(
    PrefService* pref_service,
    const WebAppRegistrar* registrar,
    const ExternallyInstalledWebAppPrefs::ParsedPrefs& parsed_data) {
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  for (auto pair : parsed_data) {
    const AppId& app_id = pair.first;
    const WebApp::ExternalConfigMap& source_to_config_map = pair.second;
    const auto& it = source_to_config_map.find(WebAppManagement::kDefault);
    // Migration will happen in the following cases:
    // 1. If app_id exists in the external prefs that had source as
    // kDefault but the app is no longer installed in the registry or if it
    // is no longer preinstalled, that means
    // it was preinstalled and then uninstalled by user.
    if (!registrar->IsInstalledByDefaultManagement(app_id) &&
        it != source_to_config_map.end()) {
      if (preinstalled_prefs.AppIdContainsAllUrls(app_id, source_to_config_map,
                                                  /*only_default=*/true)) {
        LogUserUninstalledPreinstalledAppMigration(
            UserUninstalledPreinstalledAppMigrationState::
                kPreinstalledAppDataAlreadyInSync);
      } else {
        preinstalled_prefs.Add(app_id, std::move(it->second.install_urls));
        LogUserUninstalledPreinstalledAppMigration(
            UserUninstalledPreinstalledAppMigrationState::
                kPreinstalledAppDataMigratedByUser);
      }
    }
    // 2. If the value corresponding to the app_id in
    // kWasExternalAppUninstalledByUser is true, then it was previously
    // a preinstalled app that was user uninstalled. In this case, we migrate
    // ALL install URLs for that corresponding app_id, because a preinstalled
    // app could have been installed as an app with a different source now.
    if (GetBoolWebAppPref(pref_service, app_id,
                          kWasExternalAppUninstalledByUser)) {
      if (preinstalled_prefs.AppIdContainsAllUrls(app_id, source_to_config_map,
                                                  /*only_default=*/false)) {
        LogUserUninstalledPreinstalledAppMigration(
            UserUninstalledPreinstalledAppMigrationState::
                kPreinstalledAppDataAlreadyInSync);
      } else {
        base::flat_set<GURL> urls_to_migrate =
            MergeAllUrls(source_to_config_map);
        preinstalled_prefs.Add(app_id, std::move(urls_to_migrate));
        LogUserUninstalledPreinstalledAppMigration(
            UserUninstalledPreinstalledAppMigrationState::
                kPreinstalledAppDataMigratedByOldPref);
      }
    }
  }
}

// static
base::flat_set<GURL> ExternallyInstalledWebAppPrefs::MergeAllUrls(
    const WebApp::ExternalConfigMap& source_config_map) {
  std::vector<GURL> urls;
  for (auto it : source_config_map) {
    for (const GURL& url : it.second.install_urls) {
      DCHECK(url.is_valid());
      urls.push_back(url);
    }
  }
  return urls;
}

// static
void ExternallyInstalledWebAppPrefs::LogDataMetrics(
    bool data_exists_in_pref,
    bool data_exists_in_registrar) {
  // Data in registry refers to the data stored in
  // management_to_external_config_map per web_app. See
  // WebApps::management_to_external_config_map() for more info.
  if (!data_exists_in_pref && !data_exists_in_registrar) {
    // Case 1: No external apps installed (prefs empty, empty data per web_app
    // in the registry).
    base::UmaHistogramBoolean(kPrefDataAbsentDBDataAbsent, /*sample=*/true);
  } else if (!data_exists_in_pref && data_exists_in_registrar) {
    // Case 2: prefs are empty, but data exists in registry.
    base::UmaHistogramBoolean(kPrefDataAbsentDBDataPresent, /*sample=*/true);
  } else if (data_exists_in_pref && !data_exists_in_registrar) {
    // Case 3: prefs contain data, but data does not exist in the registry.
    base::UmaHistogramBoolean(kPrefDataPresentDBDataAbsent, /*sample=*/true);
  } else {
    // Case 4: Data exists in both prefs and in the registry.
    base::UmaHistogramBoolean(kPrefDataPresentDBDataPresent, /*sample=*/true);
  }
}

}  // namespace web_app
