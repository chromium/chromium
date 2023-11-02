// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/site_search_policy_handler.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise_site_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

bool IsSiteSearchPolicyEnabled() {
  // Check that FeatureList is available as a protection against early startup
  // crashes. Some policy providers are initialized very early even before
  // base::FeatureList is available, but when policies are finally applied, the
  // feature stack is fully initialized. The instance check ensures that the
  // final decision is delayed until all features are initialized, without any
  // other downstream effect.
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(omnibox::kSiteSearchSettingsPolicy);
}

// Converts a site search policy entry |policy_dict| into a dictionary to be
// saved to prefs, with fields corresponding to |TemplateURLData|.
base::Value SiteSearchDictFromPolicyValue(
    const base::Value::Dict& policy_dict) {
  base::Value::Dict dict;

  const std::string* name =
      policy_dict.FindString(SiteSearchPolicyHandler::kName);
  CHECK(name);
  dict.Set(DefaultSearchManager::kShortName, *name);

  const std::string* shortcut =
      policy_dict.FindString(SiteSearchPolicyHandler::kShortcut);
  CHECK(shortcut);
  dict.Set(DefaultSearchManager::kKeyword, *shortcut);

  const std::string* url =
      policy_dict.FindString(SiteSearchPolicyHandler::kUrl);
  CHECK(url);
  dict.Set(DefaultSearchManager::kURL, *url);

  dict.Set(DefaultSearchManager::kCreatedByPolicy, true);
  dict.Set(DefaultSearchManager::kEnforcedByPolicy, false);

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

const std::string& GetShortcut(const base::Value& provider) {
  const std::string* shortcut =
      provider.GetDict().FindString(SiteSearchPolicyHandler::kShortcut);
  // This is safe because `SimpleSchemaValidatingPolicyHandler` guarantees that
  // the policy value is valid according to the schema.
  CHECK(shortcut);
  return *shortcut;
}

}  // namespace

const char SiteSearchPolicyHandler::kName[] = "name";
const char SiteSearchPolicyHandler::kShortcut[] = "shortcut";
const char SiteSearchPolicyHandler::kUrl[] = "url";

const int SiteSearchPolicyHandler::kMaxSiteSearchProviders = 100;

SiteSearchPolicyHandler::SiteSearchPolicyHandler(Schema schema)
    : SimpleSchemaValidatingPolicyHandler(
          key::kSiteSearchSettings,
          EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

SiteSearchPolicyHandler::~SiteSearchPolicyHandler() = default;

bool SiteSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  ignored_shortcuts_.clear();

  if (!IsSiteSearchPolicyEnabled() || !policies.Get(policy_name())) {
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

  base::flat_set<std::string> shortcuts_already_seen;
  base::flat_set<std::string> duplicated_shortcuts;
  for (const base::Value& provider : site_search_providers) {
    const std::string& shortcut = GetShortcut(provider);

    // TODO(b/306201833): Implement remaining validation rules.

    if (shortcuts_already_seen.find(shortcut) != shortcuts_already_seen.end()) {
      // Only show an error message once per shortcut.
      if (duplicated_shortcuts.find(shortcut) == duplicated_shortcuts.end()) {
        errors->AddError(policy_name(),
                         IDS_POLICY_SITE_SEARCH_SETTINGS_DUPLICATED_SHORTCUT,
                         shortcut);
      }

      duplicated_shortcuts.insert(shortcut);
      ignored_shortcuts_.insert(shortcut);
    }

    shortcuts_already_seen.insert(shortcut);
  }

  // Accept if there is at least one shortcut that should not be ignored.
  return shortcuts_already_seen.size() > ignored_shortcuts_.size();
}

void SiteSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  if (!IsSiteSearchPolicyEnabled()) {
    return;
  }

  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);

  if (!policy_value) {
    // Reset site search engines if policy was reset.
    EnterpriseSiteSearchManager::AddPrefValueToMap(base::Value::List(), prefs);
    return;
  }

  CHECK(policy_value->is_list());

  base::Value::List providers;
  for (const base::Value& item : policy_value->GetList()) {
    const std::string& shortcut = GetShortcut(item);
    if (ignored_shortcuts_.find(shortcut) == ignored_shortcuts_.end()) {
      providers.Append(SiteSearchDictFromPolicyValue(item.GetDict()));
    }
  }

  EnterpriseSiteSearchManager::AddPrefValueToMap(std::move(providers), prefs);
}

}  // namespace policy
