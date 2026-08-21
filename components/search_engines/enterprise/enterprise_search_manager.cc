// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
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
  const base::DictValue& url_dict = engine.GetDict();
  std::unique_ptr<TemplateURLData> turl_data =
      TemplateURLDataFromDictionary(url_dict);
  CHECK(turl_data);
  return turl_data;
}

}  // namespace

const char EnterpriseSearchManager::kSiteSearchSettingsPrefName[] =
    "site_search_settings.template_url_data";
const char
    EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName[] =
        "site_search_settings.overridden_keywords";

const char
    EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName[] =
        "enterprise_search_aggregator_settings.template_url_data";
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
    pref_change_registrar_.Add(
        kSiteSearchSettingsOverriddenKeywordsPrefName,
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

// Loads TemplateURLData objects from the managed preference `pref` (either
// `kSiteSearchSettingsPrefName` or
// `kEnterpriseSearchAggregatorSettingsPrefName`) into `search_engines`.
//
// For site search settings, recommended engines overridden by the user are
// filtered out. Returns the loading state (kUnavailable, kAvailableEmpty, or
// kAvailableNonEmpty).
EnterpriseSearchManager::LoadingResult
EnterpriseSearchManager::LoadSearchEnginesFromPrefs(
    const PrefService::Preference* pref,
    EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines) {
  // Only accept search engines created by enterprise policy.
  if (!pref || !pref->IsManaged()) {
    return LoadingResult::kUnavailable;
  }

  // Get the list of engines from the main policy pref.
  const base::ListValue& engine_list = pref->GetValue()->GetList();
  search_engines->reserve(search_engines->size() + engine_list.size());

  // User overrides (editing or deleting recommended search engines) are only
  // supported for site search settings.
  const bool should_check_overrides =
      pref->name() == kSiteSearchSettingsPrefName;

  LoadingResult result = LoadingResult::kAvailableEmpty;
  for (const base::Value& engine : engine_list) {
    const base::DictValue& engine_dict = engine.GetDict();
    const std::string& keyword =
        CHECK_DEREF(engine_dict.FindString(DefaultSearchManager::kKeyword));
    bool enforced_by_policy =
        engine_dict.FindBool(DefaultSearchManager::kEnforcedByPolicy)
            .value_or(false);
    // Skip recommended (non-enforced) site search engines if the user has
    // explicitly overridden / deleted them. Enforced engines always take
    // precedence over user overrides.
    if (should_check_overrides && !enforced_by_policy &&
        pref_service_
            ->GetList(EnterpriseSearchManager::
                          kSiteSearchSettingsOverriddenKeywordsPrefName)
            .contains(keyword)) {
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
      std::back_inserter(*search_engines), &DictToTemplateURLData);
  return LoadingResult::kAvailableNonEmpty;
}

void EnterpriseSearchManager::AddOverriddenKeyword(const std::string& keyword) {
  if (!pref_service_) {
    return;
  }
  ScopedListPrefUpdate overridden_keywords_update(
      pref_service_, kSiteSearchSettingsOverriddenKeywordsPrefName);
  base::ListValue& overridden_keywords_list = overridden_keywords_update.Get();

  // Prune obsolete entries from the overridden keywords preference. If a search
  // engine is no longer "in policy" (i.e. removed from the managed policy list
  // by the administrator) or has been changed to strictly enforced, its
  // override entry is stale and no longer applicable.
  const PrefService::Preference* policy_pref =
      pref_service_->FindPreference(kSiteSearchSettingsPrefName);
  if (policy_pref && policy_pref->IsManaged()) {
    base::flat_map<std::string, bool> policy_keywords_enforced_status;
    for (const base::Value& engine : policy_pref->GetValue()->GetList()) {
      const base::DictValue& engine_dict = engine.GetDict();
      const std::string& policy_keyword =
          CHECK_DEREF(engine_dict.FindString(DefaultSearchManager::kKeyword));
      bool enforced_by_policy =
          engine_dict.FindBool(DefaultSearchManager::kEnforcedByPolicy)
              .value_or(false);
      policy_keywords_enforced_status[policy_keyword] = enforced_by_policy;
    }
    overridden_keywords_list.EraseIf(
        [&policy_keywords_enforced_status](const base::Value& v) {
          auto it = policy_keywords_enforced_status.find(v.GetString());
          // Erase if the keyword is no longer defined in policy, or is now
          // enforced.
          return it == policy_keywords_enforced_status.end() || it->second;
        });
  }

  if (!overridden_keywords_list.contains(keyword)) {
    overridden_keywords_list.Append(keyword);
  }
}
