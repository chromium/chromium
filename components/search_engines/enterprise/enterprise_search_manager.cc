// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"

namespace {

std::unique_ptr<TemplateURLData> DictToTemplateURLData(
    const base::Value& engine) {
  const base::Value::Dict& url_dict = engine.GetDict();
  std::unique_ptr<TemplateURLData> turl_data =
      TemplateURLDataFromDictionary(url_dict);
  CHECK(turl_data);
  return turl_data;
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
  LoadingResult site_search_loading_result = LoadSearchEnginesFromPrefs(
      pref_service_->FindPreference(kSiteSearchSettingsPrefName),
      &search_engines);
  LoadingResult search_aggregator_loading_result =
      LoadSearchAggregator(&search_engines);
  if (site_search_loading_result != LoadingResult::kUnavailable ||
      search_aggregator_loading_result != LoadingResult::kUnavailable) {
    change_observer_.Run(std::move(search_engines));
  }
}

// Adds the search providers read from `pref` to `search_engines`. Returns true
// if `pref` exists and is managed.
EnterpriseSearchManager::LoadingResult
EnterpriseSearchManager::LoadSearchEnginesFromPrefs(
    const PrefService::Preference* pref,
    EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines) {
  // Only accept site search engine created by policy.
  if (!pref || !pref->IsManaged()) {
    return LoadingResult::kUnavailable;
  }

  LoadingResult result = LoadingResult::kAvailableEmpty;
  for (const base::Value& engine : pref->GetValue()->GetList()) {
    search_engines->emplace_back(DictToTemplateURLData(engine));
    result = LoadingResult::kAvailableNonEmpty;
  }
  return result;
}

EnterpriseSearchManager::LoadingResult
EnterpriseSearchManager::LoadSearchAggregator(
    EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines) {
  // Use the search engines created by policy if the policy is available (e.g.
  // controlling feature is enabled) and the policy value is set as a valid
  // search engine.
  LoadingResult pref_loading_result = LoadSearchEnginesFromPrefs(
      pref_service_->FindPreference(
          kEnterpriseSearchAggregatorSettingsPrefName),
      search_engines);
  if (pref_loading_result == LoadingResult::kAvailableNonEmpty) {
    return LoadingResult::kAvailableNonEmpty;
  }

  // Use pref loading result (either empty or non-empty) if there are no mock
  // search engines available.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get()
           .valid_search_engine()) {
    return pref_loading_result;
  }

  // In case the policy is not available or the policy value is empty,
  auto mock_engines = omnibox_feature_configs::SearchAggregatorProvider::Get()
                          .GetSearchEngines();
  CHECK(!mock_engines.empty());
  for (const base::Value& mock_engine : mock_engines) {
    search_engines->emplace_back(DictToTemplateURLData(mock_engine));
  }
  return LoadingResult::kAvailableNonEmpty;
}
