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
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
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
// A list to hold the keywords of site search engines which the user
// has overridden.
const char
    EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName[] =
        "site_search_settings.overridden_keywords";

// A dictionary to hold all TemplateURL data related to the enterprise search
// aggregator defined by policy.
const char
    EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName[] =
        "enterprise_search_aggregator_settings.template_url_data";
// A boolean to hold whether a shortcut is required for the enterprise search
// aggregator.
const char EnterpriseSearchManager::
    kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName[] =
        "enterprise_search_aggregator_settings.require_shortcut";

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
  registry->RegisterListPref(kSiteSearchSettingsOverriddenKeywordsPrefName);
  registry->RegisterListPref(kEnterpriseSearchAggregatorSettingsPrefName);
  registry->RegisterBooleanPref(
      kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName, false);
}

bool EnterpriseSearchManager::GetRequireShortcutValue() const {
  // Prefer mock `require_shortcut` over `require_shortcut` from pref.
  // TODO(crbug.com/402175538): Remove the ability to override pref engines via
  // feature.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get()
           .AreMockEnginesValid()) {
    // Use the `require_shortcut` preference value if set by policy.
    const PrefService::Preference* pref = pref_service_->FindPreference(
        kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName);
    return pref && pref->GetValue()->GetBool();
  }

  return omnibox_feature_configs::SearchAggregatorProvider::Get()
      .require_shortcut;
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

  // Get the list of engines from the main policy pref.
  const base::Value::List& engine_list = pref->GetValue()->GetList();
  // For site search engines, load the
  // `kSiteSearchSettingsOverriddenKeywordsPrefName` pref dictionary.
  if (base::FeatureList::IsEnabled(
          omnibox::kEnableSiteSearchAllowUserOverridePolicy) &&
      pref->name() == kSiteSearchSettingsPrefName) {
    LoadOverriddenKeywordsPref(engine_list);
  }

  LoadingResult result = LoadingResult::kAvailableEmpty;
  for (const base::Value& engine : engine_list) {
    const base::Value::Dict& engine_dict = engine.GetDict();
    const std::string* keyword =
        engine_dict.FindString(DefaultSearchManager::kKeyword);
    CHECK(keyword);
    if (base::FeatureList::IsEnabled(
            omnibox::kEnableSiteSearchAllowUserOverridePolicy) &&
        pref_service_
            ->GetList(EnterpriseSearchManager::
                          kSiteSearchSettingsOverriddenKeywordsPrefName)
            .contains(*keyword)) {
      continue;
    }
    search_engines->emplace_back(DictToTemplateURLData(engine));
    result = LoadingResult::kAvailableNonEmpty;
  }
  return result;
}

EnterpriseSearchManager::LoadingResult
EnterpriseSearchManager::LoadSearchAggregator(
    EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines) {
  // Prefer mock engines over engines from pref.
  // TODO(crbug.com/402175538): Remove the ability to override pref engines via
  // feature.
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get()
           .AreMockEnginesValid()) {
    return LoadSearchEnginesFromPrefs(
        pref_service_->FindPreference(
            kEnterpriseSearchAggregatorSettingsPrefName),
        search_engines);
  }

  // NOTE: This function assumes that `search_engines` does not contain any
  // engines that should be overridden by the feature config.
  std::ranges::transform(
      omnibox_feature_configs::SearchAggregatorProvider::Get()
          .CreateMockSearchEngines(),
      std::back_inserter(*search_engines), [](const base::Value& mock_engine) {
        return DictToTemplateURLData(mock_engine);
      });
  return LoadingResult::kAvailableNonEmpty;
}

void EnterpriseSearchManager::LoadOverriddenKeywordsPref(
    const base::Value::List& engine_list) {
  ScopedListPrefUpdate overridden_keywords_update(
      pref_service_, kSiteSearchSettingsOverriddenKeywordsPrefName);
  base::Value::List& overridden_keywords_list =
      overridden_keywords_update.Get();

  // Keep track of keywords present in the current policy along with whether
  // they are enforced by policy.
  base::flat_map<std::string, bool> policy_keywords_enforced_status;
  for (const base::Value& engine : engine_list) {
    const base::Value::Dict& engine_dict = engine.GetDict();
    const std::string* keyword =
        engine_dict.FindString(DefaultSearchManager::kKeyword);
    CHECK(keyword);
    bool enforced_by_policy =
        engine_dict.FindBool(DefaultSearchManager::kEnforcedByPolicy)
            .value_or(false);
    policy_keywords_enforced_status[*keyword] = enforced_by_policy;
  }

  // Remove keywords from the overridden keywords list for engines that are no
  // longer present in the policy or that are NOW enforced.
  overridden_keywords_list.EraseIf(
      [&policy_keywords_enforced_status](const base::Value& v) {
        auto it = policy_keywords_enforced_status.find(v.GetString());
        return it == policy_keywords_enforced_status.end() || it->second;
      });
}

void EnterpriseSearchManager::AddOverriddenKeyword(const std::string& keyword) {
  if (!pref_service_) {
    return;
  }
  ScopedListPrefUpdate overridden_keywords_update(
      pref_service_, kSiteSearchSettingsOverriddenKeywordsPrefName);
  overridden_keywords_update.Get().Append(keyword);
}
