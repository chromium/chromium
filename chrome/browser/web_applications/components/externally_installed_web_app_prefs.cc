// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"

#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
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

// Returns the base::Value in |pref_service| corresponding to our stored dict
// for |app_id|, or nullptr if it doesn't exist.
const base::Value* GetPreferenceValue(const PrefService* pref_service,
                                      const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  if (!urls_to_dicts) {
    return nullptr;
  }
  // Do a simple O(N) scan for app_id being a value in each dictionary's
  // key/value pairs. We expect both N and the number of times
  // GetPreferenceValue is called to be relatively small in practice. If they
  // turn out to be large, we can write a more sophisticated implementation.
  for (const auto& it : urls_to_dicts->DictItems()) {
    const base::Value* root = &it.second;
    const base::Value* v = root;
    if (v->is_dict()) {
      v = v->FindKey(kExtensionId);
      if (v && v->is_string() && (v->GetString() == app_id)) {
        return root;
      }
    }
  }
  return nullptr;
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
  const base::Value* v = GetPreferenceValue(pref_service, app_id);
  if (v == nullptr || !v->is_dict())
    return false;

  v = v->FindKeyOfType(kInstallSource, base::Value::Type::INTEGER);
  return (v && v->GetInt() == static_cast<int>(install_source));
}

// static
std::map<AppId, GURL> ExternallyInstalledWebAppPrefs::BuildAppIdsMap(
    const PrefService* pref_service,
    ExternalInstallSource install_source) {
  const base::DictionaryValue* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);

  std::map<AppId, GURL> ids_to_urls;

  if (!urls_to_dicts) {
    return ids_to_urls;
  }

  for (const auto& it : urls_to_dicts->DictItems()) {
    const base::Value* v = &it.second;
    if (!v->is_dict()) {
      continue;
    }

    const base::Value* install_source_value =
        v->FindKeyOfType(kInstallSource, base::Value::Type::INTEGER);
    if (!install_source_value ||
        (install_source_value->GetInt() != static_cast<int>(install_source))) {
      continue;
    }

    v = v->FindKey(kExtensionId);
    if (!v || !v->is_string()) {
      continue;
    }

    GURL url(it.first);
    DCHECK(url.is_valid() && !url.is_empty());
    ids_to_urls[v->GetString()] = url;
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

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kExtensionId, base::Value(app_id));
  dict.SetKey(kInstallSource, base::Value(static_cast<int>(install_source)));

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  update->SetKey(url.spec(), std::move(dict));
}

base::Optional<AppId> ExternallyInstalledWebAppPrefs::LookupAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* v =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKey(url.spec());
  if (v && v->is_dict()) {
    v = v->FindKey(kExtensionId);
    if (v && v->is_string()) {
      return base::make_optional(v->GetString());
    }
  }
  return base::nullopt;
}

base::Optional<AppId> ExternallyInstalledWebAppPrefs::LookupPlaceholderAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* entry =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKey(url.spec());
  if (!entry)
    return base::nullopt;

  base::Optional<bool> is_placeholder = entry->FindBoolKey(kIsPlaceholder);
  if (!is_placeholder.has_value() || !is_placeholder.value())
    return base::nullopt;

  return *entry->FindStringKey(kExtensionId);
}

void ExternallyInstalledWebAppPrefs::SetIsPlaceholder(const GURL& url,
                                                      bool is_placeholder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
             ->HasKey(url.spec()));
  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  base::Value* map = update.Get();

  auto* app_entry = map->FindKey(url.spec());
  DCHECK(app_entry);

  app_entry->SetBoolKey(kIsPlaceholder, is_placeholder);
}

bool ExternallyInstalledWebAppPrefs::IsPlaceholderApp(
    const AppId& app_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* app_prefs = GetPreferenceValue(pref_service_, app_id);
  if (!app_prefs || !app_prefs->is_dict())
    return false;
  return app_prefs->FindBoolKey(kIsPlaceholder).value_or(false);
}

}  // namespace web_app
