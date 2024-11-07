// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"

namespace {

// Adds the search providers read from `pref` to `search_engines`. Returns true
// if `pref` exists and is managed.
bool LoadSiteSearchEnginesFromPrefs(
    const PrefService::Preference* pref,
    EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines) {
  // Only accept site search engine created by policy.
  if (!pref || !pref->IsManaged()) {
    return false;
  }

  for (const base::Value& engine : pref->GetValue()->GetList()) {
    const base::Value::Dict& url_dict = engine.GetDict();
    std::unique_ptr<TemplateURLData> turl_data =
        TemplateURLDataFromDictionary(url_dict);
    CHECK(turl_data);
    search_engines->push_back(std::move(turl_data));
  }
  return true;
}

}  // namespace

// A dictionary to hold all data related to the site search engines defined by
// policy.
const char EnterpriseSearchManager::kSiteSearchSettingsPrefName[] =
    "site_search_settings.template_url_data";

// A dictionary to hold all data related to the site search engines defined by
// policy.
const char
    EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName[] =
        "enterprise_search_aggregator_settings.template_url_data";

EnterpriseSearchManager::EnterpriseSearchManager(
    PrefService* pref_service,
    const ObserverCallback& change_observer)
    : pref_service_(pref_service), change_observer_(change_observer) {
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        kSiteSearchSettingsPrefName,
        base::BindRepeating(&EnterpriseSearchManager::OnPrefChanged,
                            base::Unretained(this)));
    if (base::FeatureList::IsEnabled(omnibox::kEnableSearchAggregatorPolicy)) {
      pref_change_registrar_.Add(
          kEnterpriseSearchAggregatorSettingsPrefName,
          base::BindRepeating(&EnterpriseSearchManager::OnPrefChanged,
                              base::Unretained(this)));
    }
    OnPrefChanged();
  }
}

EnterpriseSearchManager::~EnterpriseSearchManager() = default;

// static
void EnterpriseSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kSiteSearchSettingsPrefName);
  registry->RegisterListPref(kEnterpriseSearchAggregatorSettingsPrefName);
}

void EnterpriseSearchManager::OnPrefChanged() {
  if (!change_observer_) {
    return;
  }

  EnterpriseSearchManager::OwnedTemplateURLDataVector search_engines;
  bool valid_site_search = LoadSiteSearchEnginesFromPrefs(
      pref_service_->FindPreference(kSiteSearchSettingsPrefName),
      &search_engines);
  bool valid_aggregator = LoadSiteSearchEnginesFromPrefs(
      pref_service_->FindPreference(
          kEnterpriseSearchAggregatorSettingsPrefName),
      &search_engines);
  if (valid_site_search || valid_aggregator) {
    change_observer_.Run(std::move(search_engines));
  }
}
