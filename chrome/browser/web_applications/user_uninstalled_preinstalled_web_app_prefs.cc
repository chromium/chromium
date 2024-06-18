// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

const char UserUninstalledPreinstalledWebAppPrefs::
    kUserUninstalledPreinstalledAppAction[] =
        "WebApp.Preinstalled.UninstallByUser";

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
    const webapps::AppId& app_id,
    base::flat_set<GURL> install_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value::List url_list;

  AppendExistingInstallUrlsPerAppId(app_id, install_urls);

  for (auto install_url : install_urls)
    url_list.Append(install_url.spec());

  if (!DoesAppIdExist(app_id)) {
    base::RecordAction(
        base::UserMetricsAction(kUserUninstalledPreinstalledAppAction));
  }

  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kUserUninstalledPreinstalledWebAppPref);
  update->Set(app_id, std::move(url_list));
}

std::optional<webapps::AppId>
UserUninstalledPreinstalledWebAppPrefs::LookUpAppIdByInstallUrl(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!url.is_valid())
    return std::nullopt;

  for (auto it : ids_to_urls) {
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
  return std::nullopt;
}

bool UserUninstalledPreinstalledWebAppPrefs::DoesAppIdExist(
    const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  return ids_to_urls.contains(app_id);
}

void UserUninstalledPreinstalledWebAppPrefs::AppendExistingInstallUrlsPerAppId(
    const webapps::AppId& app_id,
    base::flat_set<GURL>& urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  if (!ids_to_urls.contains(app_id))
    return;

  const base::Value::List* current_list = ids_to_urls.FindList(app_id);
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
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  return ids_to_urls.size();
}

bool UserUninstalledPreinstalledWebAppPrefs::RemoveByInstallUrl(
    const webapps::AppId& app_id,
    const GURL& install_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  // Prefs are empty, so no need of removal.

  // Pref does not contain the app_id, so no need of removal.
  if (!ids_to_urls.contains(app_id))
    return false;

  const base::Value::List* url_list = ids_to_urls.FindList(app_id);
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

  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kUserUninstalledPreinstalledWebAppPref);

  // Add the URLs back to the pref and clear pref in case there are
  // app_ids with empty URLs.
  if (install_urls.size() == 0)
    return update->Remove(app_id);

  // Add the remaining URLs to the preinstalled prefs after deletion.
  update->Set(app_id, std::move(install_urls));
  return true;
}

bool UserUninstalledPreinstalledWebAppPrefs::RemoveByAppId(
    const webapps::AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  // Pref does not contain the app_id, so no need of removal.
  if (!ids_to_urls.contains(app_id))
    return false;
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kUserUninstalledPreinstalledWebAppPref);
  return update->Remove(app_id);
}

bool UserUninstalledPreinstalledWebAppPrefs::AppIdContainsAllUrls(
    const webapps::AppId& app_id,
    const base::flat_map<WebAppManagement::Type,
                         WebApp::ExternalManagementConfig>& url_map,
    const bool only_default) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (url_map.empty())
    return false;

  const base::Value::Dict& ids_to_urls =
      pref_service_->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref);

  const base::Value::List* current_list = ids_to_urls.FindList(app_id);
  if (!current_list)
    return false;

  base::flat_set<std::string> existing_urls;
  for (const base::Value& url : *current_list) {
    existing_urls.emplace(url.GetString());
  }

  for (auto it : url_map) {
    if (only_default && !(it.first == WebAppManagement::kDefault))
      continue;

    for (const GURL& url_to_insert : it.second.install_urls) {
      if (!base::Contains(existing_urls, url_to_insert.spec()))
        return false;
    }
  }
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void UserUninstalledPreinstalledWebAppPrefs::ClearAllApps() {
  pref_service_->ClearPref(prefs::kUserUninstalledPreinstalledWebAppPref);
}
#endif

}  // namespace web_app
