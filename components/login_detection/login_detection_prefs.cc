// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login_detection/login_detection_prefs.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/login_detection/login_detection_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace login_detection::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kOAuthSignedInSitesPref);
}

void RemoveLoginDetectionData(PrefService* prefs) {
  prefs->ClearPref(kOAuthSignedInSitesPref);
}

void SaveSiteToOAuthSignedInList(PrefService* pref_service, const GURL& url) {
  ScopedDictPrefUpdate update(pref_service, kOAuthSignedInSitesPref);
  base::DictValue& dict = update.Get();
  dict.Set(GetSiteNameForURL(url), base::TimeToValue(base::Time::Now()));

  // Try making space by removing sites having invalid sign-in time. This should
  // not happen unless the pref is corrupt somehow.
  if (dict.size() > GetOauthLoggedInSitesMaxSize()) {
    std::vector<std::string> invalid_sites;
    for (auto site_entry : dict) {
      if (!base::ValueToTime(site_entry.second)) {
        invalid_sites.push_back(site_entry.first);
      }
    }
    for (const auto& invalid_site : invalid_sites) {
      dict.Remove(invalid_site);
    }
  }

  // Limit the dict to its allowed max size, by removing the site entries which
  // are signed-in the earliest.
  while (dict.size() > GetOauthLoggedInSitesMaxSize()) {
    // Holds the pair of site name, its last login time for the site that was
    // least recently signed-in to be removed.
    std::optional<std::pair<std::string, base::Time>> site_entry_to_remove;
    for (auto site_entry : dict) {
      base::Time signin_time = *base::ValueToTime(site_entry.second);
      if (!site_entry_to_remove || signin_time < site_entry_to_remove->second) {
        site_entry_to_remove = std::make_pair(site_entry.first, signin_time);
      }
    }
    dict.Remove(site_entry_to_remove->first);
  }
}

bool IsSiteInOAuthSignedInList(PrefService* pref_service, const GURL& url) {
  return pref_service->GetDict(kOAuthSignedInSitesPref)
      .contains(GetSiteNameForURL(url));
}

std::vector<url::Origin> GetOAuthSignedInSites(PrefService* pref_service) {
  std::vector<url::Origin> sites;
  for (const auto site_entry : pref_service->GetDict(kOAuthSignedInSitesPref)) {
    sites.push_back(url::Origin::Create(GURL(site_entry.first)));
  }
  return sites;
}

}  // namespace login_detection::prefs
