// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise_site_search_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"

// A dictionary to hold all data related to the site search engines defined by
// policy.
const char EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName[] =
    "site_search_settings.template_url_data";

EnterpriseSiteSearchManager::EnterpriseSiteSearchManager(
    PrefService* pref_service,
    const ObserverCallback& change_observer)
    : pref_service_(pref_service), change_observer_(change_observer) {
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        kSiteSearchSettingsPrefName,
        base::BindRepeating(
            &EnterpriseSiteSearchManager::OnSiteSearchPrefChanged,
            base::Unretained(this)));
  }
}

EnterpriseSiteSearchManager::~EnterpriseSiteSearchManager() = default;

// static
void EnterpriseSiteSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  CHECK(base::FeatureList::IsEnabled(omnibox::kSiteSearchSettingsPolicy));

  registry->RegisterListPref(kSiteSearchSettingsPrefName);
}

// static
void EnterpriseSiteSearchManager::AddPrefValueToMap(
    base::Value::List providers,
    PrefValueMap* pref_value_map) {
  CHECK(base::FeatureList::IsEnabled(omnibox::kSiteSearchSettingsPolicy));

  pref_value_map->SetValue(kSiteSearchSettingsPrefName,
                           base::Value(std::move(providers)));
}

void EnterpriseSiteSearchManager::LoadSiteSearchEnginesFromPrefs(
    const PrefService::Preference* pref) {
  site_search_engines_.clear();

  const base::Value::List& site_search_engines =
      pref_service_->GetList(kSiteSearchSettingsPrefName);
  if (site_search_engines.empty()) {
    return;
  }

  for (const base::Value& engine : site_search_engines) {
    const base::Value::Dict& url_dict = engine.GetDict();
    std::unique_ptr<TemplateURLData> turl_data =
        TemplateURLDataFromDictionary(url_dict);
    CHECK(turl_data);
    site_search_engines_.push_back(std::move(turl_data));
  }
}

void EnterpriseSiteSearchManager::OnSiteSearchPrefChanged() {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kSiteSearchSettingsPrefName);
  CHECK(pref);

  // Only accept site search engine created by policy.
  if (!pref->IsManaged()) {
    return;
  }

  LoadSiteSearchEnginesFromPrefs(pref);

  if (change_observer_) {
    change_observer_.Run(site_search_engines_);
  }
}
