// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

UserUninstalledPreinstalledWebAppPrefs::UserUninstalledPreinstalledWebAppPrefs(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

// static
void UserUninstalledPreinstalledWebAppPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kUserUninstalledPreinstalledWebAppPref);
}

void UserUninstalledPreinstalledWebAppPrefs::Add(
    const AppId& app_id,
    base::flat_set<GURL> install_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List url_list;

  AppendExistingInstallUrlsPerAppId(app_id, install_urls);

  for (auto install_url : install_urls)
    url_list.Append(install_url.spec());

  DictionaryPrefUpdate update(pref_service_,
                              prefs::kUserUninstalledPreinstalledWebAppPref);
  update->SetKey(app_id, base::Value(std::move(url_list)));
}

absl::optional<AppId>
UserUninstalledPreinstalledWebAppPrefs::LookUpAppIdByInstallUrl(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* ids_to_urls = pref_service_->GetDictionary(
      prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!ids_to_urls || !url.is_valid())
    return absl::nullopt;

  for (auto it : ids_to_urls->DictItems()) {
    const base::Value::List* urls = it.second.GetIfList();
    if (!urls)
      continue;
    for (const base::Value& link : *urls) {
      GURL install_url(link.GetString());
      DCHECK(install_url.is_valid());
      if (install_url == url)
        return it.first;
    }
  }
  return absl::nullopt;
}

bool UserUninstalledPreinstalledWebAppPrefs::DoesAppIdExist(
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* ids_to_urls = pref_service_->GetDictionary(
      prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!ids_to_urls)
    return false;

  const base::Value::Dict* pref_info = ids_to_urls->GetIfDict();
  return pref_info && pref_info->contains(app_id);
}

void UserUninstalledPreinstalledWebAppPrefs::AppendExistingInstallUrlsPerAppId(
    const AppId& app_id,
    base::flat_set<GURL>& urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* ids_to_urls = pref_service_->GetDictionary(
      prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!ids_to_urls)
    return;

  const base::Value::Dict* pref_info = ids_to_urls->GetIfDict();
  if (!pref_info || !pref_info->contains(app_id))
    return;

  const base::Value::List* current_list = pref_info->FindList(app_id);
  if (!current_list)
    return;

  for (const base::Value& url : *current_list) {
    // This is being done so as to remove duplicate urls from being
    // added to the list.
    DCHECK(GURL(url.GetString()).is_valid());
    urls.emplace(url.GetString());
  }
}

int UserUninstalledPreinstalledWebAppPrefs::Size() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* ids_to_urls = pref_service_->GetDictionary(
      prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!ids_to_urls)
    return 0;

  const base::Value::Dict* pref_info = ids_to_urls->GetIfDict();
  return pref_info->size();
}

bool UserUninstalledPreinstalledWebAppPrefs::RemoveByInstallUrl(
    const AppId& app_id,
    const GURL& install_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* ids_to_urls = pref_service_->GetDictionary(
      prefs::kUserUninstalledPreinstalledWebAppPref);

  // Prefs are empty, so no need of removal.
  if (!ids_to_urls)
    return false;

  // Pref does not contain the app_id, so no need of removal.
  const base::Value::Dict& pref_info = ids_to_urls->GetDict();
  if (!pref_info.contains(app_id))
    return false;

  const base::Value::List* url_list = pref_info.FindList(app_id);
  base::Value::List install_urls;
  for (const base::Value& url : *url_list) {
    const std::string* url_str = url.GetIfString();
    if (!url_str || (url_str && *url_str == install_url.spec()))
      continue;
    install_urls.Append(*url_str);
  }

  // This means the URL did not exist and was hence not deleted.
  if (install_urls.size() == url_list->size())
    return false;

  DictionaryPrefUpdate update(pref_service_,
                              prefs::kUserUninstalledPreinstalledWebAppPref);

  // Add the URLs back to the pref and clear pref in case there are
  // app_ids with empty URLs.
  if (install_urls.size() == 0)
    return update->RemoveKey(app_id);

  // Add the remaining URLs to the preinstalled prefs after deletion.
  update->SetKey(app_id, base::Value(std::move(install_urls)));
  return true;
}

}  // namespace web_app
