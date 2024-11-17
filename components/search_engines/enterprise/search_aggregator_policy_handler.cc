// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/search_aggregator_policy_handler.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/enterprise/search_engine_fields_validators.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

namespace {

bool IsPolicyEnabled() {
  // Check that FeatureList is available as a protection against early startup
  // crashes. Some policy providers are initialized very early even before
  // base::FeatureList is available, but when policies are finally applied, the
  // feature stack is fully initialized. The instance check ensures that the
  // final decision is delayed until all features are initialized, without any
  // other downstream effect.
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(omnibox::kEnableSearchAggregatorPolicy);
}

bool UrlIsNotHttps(const std::string& policy_name,
                   const std::string& url,
                   PolicyErrorMap* errors) {
  GURL gurl(url);
  if (gurl.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  errors->AddError(policy_name, IDS_POLICY_SITE_SEARCH_SETTINGS_URL_NOT_HTTPS,
                   url);
  return true;
}

// Converts a search aggregator policy value `policy_dict` into a dictionary to
// be saved to prefs, with fields corresponding to `TemplateURLData`.
base::Value SearchAggregatorDictFromPolicyValue(
    const base::Value::Dict& policy_dict,
    bool featured) {
  base::Value::Dict dict;

  const std::string* name =
      policy_dict.FindString(SearchAggregatorPolicyHandler::kName);
  CHECK(name);
  dict.Set(DefaultSearchManager::kShortName, *name);

  const std::string* shortcut =
      policy_dict.FindString(SearchAggregatorPolicyHandler::kShortcut);
  CHECK(shortcut);
  dict.Set(DefaultSearchManager::kKeyword,
           featured ? "@" + *shortcut : *shortcut);

  const std::string* search_url =
      policy_dict.FindString(SearchAggregatorPolicyHandler::kSearchUrl);
  CHECK(search_url);
  dict.Set(DefaultSearchManager::kURL, *search_url);

  const std::string* suggest_url =
      policy_dict.FindString(SearchAggregatorPolicyHandler::kSuggestUrl);
  CHECK(suggest_url);
  dict.Set(DefaultSearchManager::kSuggestionsURL, *suggest_url);

  const std::string* icon_url =
      policy_dict.FindString(SearchAggregatorPolicyHandler::kIconUrl);
  if (icon_url) {
    dict.Set(DefaultSearchManager::kFaviconURL, *icon_url);
  }

  dict.Set(
      DefaultSearchManager::kCreatedByPolicy,
      static_cast<int>(TemplateURLData::CreatedByPolicy::kSearchAggregator));
  dict.Set(DefaultSearchManager::kEnforcedByPolicy, false);
  dict.Set(DefaultSearchManager::kFeaturedByPolicy, featured);
  dict.Set(DefaultSearchManager::kIsActive,
           static_cast<int>(TemplateURLData::ActiveStatus::kTrue));
  dict.Set(DefaultSearchManager::kSafeForAutoReplace, false);

  double timestamp = static_cast<double>(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  dict.Set(DefaultSearchManager::kDateCreated, timestamp);
  dict.Set(DefaultSearchManager::kLastModified, timestamp);

  return base::Value(std::move(dict));
}

}  // namespace

const char SearchAggregatorPolicyHandler::kIconUrl[] = "icon_url";
const char SearchAggregatorPolicyHandler::kName[] = "name";
const char SearchAggregatorPolicyHandler::kSearchUrl[] = "search_url";
const char SearchAggregatorPolicyHandler::kShortcut[] = "shortcut";
const char SearchAggregatorPolicyHandler::kSuggestUrl[] = "suggest_url";

// TODO(375240486): Define the path to the pref where the policy will be saved.
SearchAggregatorPolicyHandler::SearchAggregatorPolicyHandler(Schema schema)
    : SimpleSchemaValidatingPolicyHandler(
          key::kEnterpriseSearchAggregatorSettings,
          EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

SearchAggregatorPolicyHandler::~SearchAggregatorPolicyHandler() = default;

bool SearchAggregatorPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!IsPolicyEnabled() || !policies.Get(policy_name())) {
    return true;
  }

  if (!SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                errors)) {
    return false;
  }

  const base::Value::Dict& search_aggregator =
      policies.GetValue(policy_name(), base::Value::Type::DICT)->GetDict();

  // Shortcut validation.
  const std::string& shortcut = *search_aggregator.FindString(kShortcut);
  if (search_engine_fields_validators::ShortcutIsEmpty(policy_name(), shortcut,
                                                       errors) ||
      search_engine_fields_validators::ShortcutHasWhitespace(
          policy_name(), shortcut, errors) ||
      search_engine_fields_validators::ShortcutStartsWithAtSymbol(
          policy_name(), shortcut, errors) ||
      search_engine_fields_validators::
          ShortcutEqualsDefaultSearchProviderKeyword(policy_name(), shortcut,
                                                     policies, errors)) {
    return false;
  }

  // Name validation.
  if (search_engine_fields_validators::NameIsEmpty(
          policy_name(), *search_aggregator.FindString(kName), errors)) {
    return false;
  }

  // Search URL validation.
  const std::string& search_url = *search_aggregator.FindString(kSearchUrl);
  if (search_engine_fields_validators::UrlIsEmpty(policy_name(), search_url,
                                                  errors) ||
      UrlIsNotHttps(policy_name(), search_url, errors) ||
      search_engine_fields_validators::ReplacementStringIsMissingFromUrl(
          policy_name(), search_url, errors)) {
    return false;
  }

  // Suggest URL validation.
  const std::string& suggest_url = *search_aggregator.FindString(kSuggestUrl);
  if (search_engine_fields_validators::UrlIsEmpty(policy_name(), suggest_url,
                                                  errors) ||
      UrlIsNotHttps(policy_name(), suggest_url, errors)) {
    return false;
  }

  // UrlIsEmpty is not used here because icon_url is optional and so there is no
  // need to record an error if it is empty.
  const std::string* icon_url = search_aggregator.FindString(kIconUrl);
  if (icon_url && !icon_url->empty() &&
      UrlIsNotHttps(policy_name(), *icon_url, errors)) {
    return false;
  }

  return true;
}

void SearchAggregatorPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!IsPolicyEnabled()) {
    return;
  }

  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!policy_value) {
    // Reset search aggregator if policy was reset.
    prefs->SetValue(
        EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
        base::Value(base::Value::List()));
    return;
  }

  base::Value::List providers;
  providers.Append(SearchAggregatorDictFromPolicyValue(policy_value->GetDict(),
                                                       /*featured=*/false));
  providers.Append(SearchAggregatorDictFromPolicyValue(policy_value->GetDict(),
                                                       /*featured=*/true));

  prefs->SetValue(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      base::Value(std::move(providers)));
}

}  // namespace policy
