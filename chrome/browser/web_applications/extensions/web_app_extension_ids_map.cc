// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// The stored preferences have evolved over time:
//
// For M70 (branched August 2018):
//
// "web_apps": {
//   "extension_ids": {
//     "https://events.google.com/io2016/?utm_source=web_app_manifest":
//         "mjgafbdfajpigcjmkgmeokfbodbcfijl",
//     "https://www.chromestatus.com/features":
//         "fedbieoalmbobgfjapopkghdmhgncnaa"
//   }
// }
//
// For M71 and above:
//
// "web_apps": {
//   "extension_ids": {
//     "https://events.google.com/io2016/?utm_source=web_app_manifest": {
//       "extension_id": "mjgafbdfajpigcjmkgmeokfbodbcfijl",
//       "install_source": 1
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
// Two levels in is a dictionary (key/value pairs) whose keys are URLs:
//  - For M70, the values are strings.
//  - For M71+, values are leaf dictionaries. Those leaf dictionaries have keys
//    such as kExtensionId and kInstallSource.
constexpr char kExtensionId[] = "extension_id";
constexpr char kInstallSource[] = "install_source";

constexpr InstallSource m70_implicit_install_source = InstallSource::kInternal;

}  // namespace

// static
void ExtensionIdsMap::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppsExtensionIDs);
}

// static
void ExtensionIdsMap::UpgradeFromM70Format(PrefService* pref_service) {
  const base::DictionaryValue* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  if (!urls_to_dicts) {
    return;
  }

  // The std::pair is [url, extension_id].
  std::vector<std::pair<std::string, std::string>> upgrades;
  for (const auto& it : urls_to_dicts->DictItems()) {
    if (it.second.is_string()) {
      upgrades.emplace_back(it.first, it.second.GetString());
    }
  }
  if (upgrades.empty()) {
    return;
  }

  DictionaryPrefUpdate update(pref_service, prefs::kWebAppsExtensionIDs);
  for (const auto& it : upgrades) {
    // Write the M71+ format.
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey(kExtensionId, base::Value(it.second));
    dict.SetKey(kInstallSource,
                base::Value(static_cast<int>(m70_implicit_install_source)));

    update->SetKey(it.first, std::move(dict));
  }
}

// static
bool ExtensionIdsMap::HasExtensionId(const PrefService* pref_service,
                                     const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  if (!urls_to_dicts) {
    return false;
  }
  // Do a simple O(N) scan for extension_id being a value in each dictionary's
  // key/value pairs. We expect both N and the number of times HasExtensionId
  // is called to be relatively small in practice. If they turn out to be
  // large, we can write a more sophisticated implementation.
  for (const auto& it : urls_to_dicts->DictItems()) {
    // On M70, v should be a string. On M71+, v should be a dict.
    const base::Value* v = &it.second;
    if (v->is_dict()) {
      v = v->FindKey(kExtensionId);
    }

    // On both M70 and M71+, v should now be the extension_id.
    if (v && v->is_string() && (v->GetString() == extension_id)) {
      return true;
    }
  }
  return false;
}

// static
std::vector<GURL> ExtensionIdsMap::GetInstalledAppUrls(
    Profile* profile,
    InstallSource install_source) {
  const base::DictionaryValue* urls_to_dicts =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  std::vector<GURL> installed_app_urls;

  if (!urls_to_dicts) {
    return installed_app_urls;
  }

  for (const auto& it : urls_to_dicts->DictItems()) {
    // On M70, v should be a string. On M71+, v should be a dict.
    //
    // For M70, the implicit install_source is kInternal.
    const base::Value* v = &it.second;
    auto actual_install_source = m70_implicit_install_source;
    if (v->is_dict()) {
      const base::Value* install_source_value =
          v->FindKeyOfType(kInstallSource, base::Value::Type::INTEGER);
      if (!install_source_value) {
        continue;
      }
      actual_install_source =
          static_cast<InstallSource>(install_source_value->GetInt());

      v = v->FindKey(kExtensionId);
    }

    // On both M70 and M71+, v should now be the extension_id.
    if (!v || !v->is_string() || (actual_install_source != install_source)) {
      continue;
    }
    auto* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            v->GetString(), extensions::ExtensionRegistry::EVERYTHING);
    if (!extension) {
      continue;
    }

    installed_app_urls.emplace_back(it.first);
  }

  return installed_app_urls;
}

ExtensionIdsMap::ExtensionIdsMap(PrefService* pref_service)
    : pref_service_(pref_service) {
  UpgradeFromM70Format(pref_service);
}

void ExtensionIdsMap::Insert(const GURL& url,
                             const std::string& extension_id,
                             InstallSource install_source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Write the M71+ format.
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kExtensionId, base::Value(extension_id));
  dict.SetKey(kInstallSource, base::Value(static_cast<int>(install_source)));

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  update->SetKey(url.spec(), std::move(dict));
}

base::Optional<std::string> ExtensionIdsMap::LookupExtensionId(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // On M70, v should be a string. On M71+, v should be a dict.
  const base::Value* v =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKey(url.spec());
  if (v && v->is_dict()) {
    v = v->FindKey(kExtensionId);
  }

  // On both M70 and M71+, v should now be the extension_id.
  if (v && v->is_string()) {
    return base::make_optional(v->GetString());
  }
  return base::nullopt;
}

}  // namespace web_app
