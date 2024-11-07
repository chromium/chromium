// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/site_search_policy_handler.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/enterprise/search_engine_fields_validators.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Converts a site search policy entry `policy_dict` into a dictionary to be
// saved to prefs, with fields corresponding to `TemplateURLData`.
base::Value SiteSearchDictFromPolicyValue(const base::Value::Dict& policy_dict,
                                          bool featured) {
  base::Value::Dict dict;

  const std::string* name =
      policy_dict.FindString(SiteSearchPolicyHandler::kName);
  CHECK(name);
  dict.Set(DefaultSearchManager::kShortName, *name);

  const std::string* shortcut =
      policy_dict.FindString(SiteSearchPolicyHandler::kShortcut);
  CHECK(shortcut);
  dict.Set(DefaultSearchManager::kKeyword,
           featured ? "@" + *shortcut : *shortcut);

  const std::string* url =
      policy_dict.FindString(SiteSearchPolicyHandler::kUrl);
  CHECK(url);
  dict.Set(DefaultSearchManager::kURL, *url);

  dict.Set(DefaultSearchManager::kFeaturedByPolicy, featured);

  dict.Set(DefaultSearchManager::kCreatedByPolicy,
           static_cast<int>(TemplateURLData::CreatedByPolicy::kSiteSearch));
  dict.Set(DefaultSearchManager::kEnforcedByPolicy, false);
  dict.Set(DefaultSearchManager::kIsActive,
           static_cast<int>(TemplateURLData::ActiveStatus::kTrue));

  // TODO(b/307543761): Create a new field `featured_by_policy` and setting
  // according to the corresponding dictionary field.

  dict.Set(DefaultSearchManager::kFaviconURL,
           TemplateURL::GenerateFaviconURL(GURL(*url)).spec());

  dict.Set(DefaultSearchManager::kSafeForAutoReplace, false);

  double timestamp = static_cast<double>(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  dict.Set(DefaultSearchManager::kDateCreated, timestamp);
  dict.Set(DefaultSearchManager::kLastModified, timestamp);

  return base::Value(std::move(dict));
}

void WarnIfNonHttpsUrl(const std::string& policy_name,
                       const std::string& url,
                       PolicyErrorMap* errors) {
  GURL gurl(url);
  if (!gurl.SchemeIs(url::kHttpsScheme)) {
    errors->AddError(policy_name, IDS_POLICY_SITE_SEARCH_SETTINGS_URL_NOT_HTTPS,
                     url);
  }
}

bool ShortcutAlreadySeen(
    const std::string& policy_name,
    const std::string& shortcut,
    const base::flat_set<std::string>& shortcuts_already_seen,
    PolicyErrorMap* errors,
    base::flat_set<std::string>* duplicated_shortcuts) {
  if (shortcuts_already_seen.find(shortcut) == shortcuts_already_seen.end()) {
    return false;
  }

  if (duplicated_shortcuts->find(shortcut) == duplicated_shortcuts->end()) {
    duplicated_shortcuts->insert(shortcut);

    // Only show an error message once per shortcut.
    errors->AddError(policy_name,
                     IDS_POLICY_SITE_SEARCH_SETTINGS_DUPLICATED_SHORTCUT,
                     shortcut);
  }
  return true;
}

}  // namespace

const char SiteSearchPolicyHandler::kName[] = "name";
const char SiteSearchPolicyHandler::kShortcut[] = "shortcut";
const char SiteSearchPolicyHandler::kUrl[] = "url";
const char SiteSearchPolicyHandler::kFeatured[] = "featured";

const int SiteSearchPolicyHandler::kMaxSiteSearchProviders = 100;
const int SiteSearchPolicyHandler::kMaxFeaturedProviders = 3;

SiteSearchPolicyHandler::SiteSearchPolicyHandler(Schema schema)
    : SimpleSchemaValidatingPolicyHandler(
          key::kSiteSearchSettings,
          EnterpriseSearchManager::kSiteSearchSettingsPrefName,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

SiteSearchPolicyHandler::~SiteSearchPolicyHandler() = default;

bool SiteSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  ignored_shortcuts_.clear();

  if (!policies.Get(policy_name())) {
    return true;
  }

  if (!SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                errors)) {
    return false;
  }

  const base::Value::List& site_search_providers =
      policies.GetValue(policy_name(), base::Value::Type::LIST)->GetList();

  if (site_search_providers.size() > kMaxSiteSearchProviders) {
    errors->AddError(policy_name(),
                     IDS_POLICY_SITE_SEARCH_SETTINGS_MAX_PROVIDERS_LIMIT_ERROR,
                     base::NumberToString(kMaxSiteSearchProviders));
    return false;
  }

  int num_featured = base::ranges::count_if(
      site_search_providers, [](const base::Value& provider) {
        return provider.GetDict().FindBool(kFeatured).value_or(false);
      });
  if (num_featured > kMaxFeaturedProviders) {
    errors->AddError(
        policy_name(),
        IDS_POLICY_SITE_SEARCH_SETTINGS_MAX_FEATURED_PROVIDERS_LIMIT_ERROR,
        base::NumberToString(kMaxFeaturedProviders));
    return false;
  }

  base::flat_set<std::string> shortcuts_already_seen;
  base::flat_set<std::string> duplicated_shortcuts;
  for (const base::Value& provider : site_search_providers) {
    const base::Value::Dict& provider_dict = provider.GetDict();
    const std::string& shortcut = *provider_dict.FindString(kShortcut);
    const std::string& url = *provider_dict.FindString(kUrl);

    bool invalid_entry =
        search_engine_fields_validators::ShortcutIsEmpty(policy_name(),
                                                         shortcut, errors) ||
        search_engine_fields_validators::NameIsEmpty(
            policy_name(), *provider_dict.FindString(kName), errors) ||
        search_engine_fields_validators::UrlIsEmpty(policy_name(), url,
                                                    errors) ||
        search_engine_fields_validators::ShortcutHasWhitespace(
            policy_name(), shortcut, errors) ||
        search_engine_fields_validators::ShortcutStartsWithAtSymbol(
            policy_name(), shortcut, errors) ||
        search_engine_fields_validators::
            ShortcutEqualsDefaultSearchProviderKeyword(policy_name(), shortcut,
                                                       policies, errors) ||
        ShortcutAlreadySeen(policy_name(), shortcut, shortcuts_already_seen,
                            errors, &duplicated_shortcuts) ||
        search_engine_fields_validators::ReplacementStringIsMissingFromUrl(
            policy_name(), url, errors);

    if (invalid_entry) {
      ignored_shortcuts_.insert(shortcut);
    } else {
      WarnIfNonHttpsUrl(policy_name(), url, errors);
    }

    shortcuts_already_seen.insert(shortcut);
  }

  // Accept if there is at least one shortcut that should not be ignored.
  if (shortcuts_already_seen.size() > ignored_shortcuts_.size()) {
    return true;
  }

  errors->AddError(policy_name(),
                   IDS_POLICY_SITE_SEARCH_SETTINGS_NO_VALID_PROVIDER);
  return false;
}

void SiteSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);

  if (!policy_value) {
    // Reset site search engines if policy was reset.
    prefs->SetValue(EnterpriseSearchManager::kSiteSearchSettingsPrefName,
                    base::Value(base::Value::List()));
    return;
  }

  CHECK(policy_value->is_list());

  base::Value::List providers;
  for (const base::Value& item : policy_value->GetList()) {
      const base::Value::Dict& policy_dict = item.GetDict();
      const std::string& shortcut = *policy_dict.FindString(kShortcut);
      if (ignored_shortcuts_.find(shortcut) == ignored_shortcuts_.end()) {
        providers.Append(
            SiteSearchDictFromPolicyValue(policy_dict, /*featured=*/false));
        if (policy_dict.FindBool(kFeatured).value_or(false)) {
          providers.Append(SiteSearchDictFromPolicyValue(policy_dict,
                                                         /*featured=*/true));
        }
      }
  }

  prefs->SetValue(EnterpriseSearchManager::kSiteSearchSettingsPrefName,
                  base::Value(std::move(providers)));
}

}  // namespace policy
