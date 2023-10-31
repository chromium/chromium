// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/site_search_policy_handler.h"

#include "base/feature_list.h"
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

}  // namespace

const char SiteSearchPolicyHandler::kName[] = "name";
const char SiteSearchPolicyHandler::kShortcut[] = "shortcut";
const char SiteSearchPolicyHandler::kUrl[] = "url";

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
  if (!IsSiteSearchPolicyEnabled() || !policies.Get(policy_name())) {
    return true;
  }

  // TODO(b/306201833): Validate the policy value.
  return SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors);
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
    providers.Append(SiteSearchDictFromPolicyValue(item.GetDict()));
  }

  // TODO(b/306201833): Only copy over the valid entries.
  EnterpriseSiteSearchManager::AddPrefValueToMap(std::move(providers), prefs);
}

}  // namespace policy
