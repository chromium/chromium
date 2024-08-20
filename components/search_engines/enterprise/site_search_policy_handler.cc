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
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
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

const std::string& GetField(const base::Value& provider,
                            const char* field_name) {
  const std::string* value = provider.GetDict().FindString(field_name);
  // This is safe because `SimpleSchemaValidatingPolicyHandler` guarantees that
  // the policy value is valid according to the schema.
  CHECK(value);
  return *value;
}

const std::string& GetShortcut(const base::Value& provider) {
  return GetField(provider, SiteSearchPolicyHandler::kShortcut);
}

const std::string& GetName(const base::Value& provider) {
  return GetField(provider, SiteSearchPolicyHandler::kName);
}

const std::string& GetUrl(const base::Value& provider) {
  return GetField(provider, SiteSearchPolicyHandler::kUrl);
}

bool ShortcutIsEmpty(const std::string& policy_name,
                     const std::string& shortcut,
                     PolicyErrorMap* errors) {
  if (!shortcut.empty()) {
    return false;
  }

  errors->AddError(policy_name,
                   IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_IS_EMPTY);
  return true;
}

bool NameIsEmpty(const std::string& policy_name,
                 const std::string& name,
                 PolicyErrorMap* errors) {
  if (!name.empty()) {
    return false;
  }

  errors->AddError(policy_name, IDS_POLICY_SITE_SEARCH_SETTINGS_NAME_IS_EMPTY);
  return true;
}

bool UrlIsEmpty(const std::string& policy_name,
                const std::string& url,
                PolicyErrorMap* errors) {
  if (!url.empty()) {
    return false;
  }

  errors->AddError(policy_name, IDS_POLICY_SITE_SEARCH_SETTINGS_URL_IS_EMPTY);
  return true;
}

bool ShortcutHasWhitespace(const std::string& policy_name,
                           const std::string& shortcut,
                           PolicyErrorMap* errors) {
  if (shortcut.find_first_of(base::kWhitespaceASCII) == std::u16string::npos) {
    return false;
  }

  errors->AddError(policy_name,
                   IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_CONTAINS_SPACE,
                   shortcut);
  return true;
}

bool ShortcutStartsWithAtSymbol(const std::string& policy_name,
                                const std::string& shortcut,
                                PolicyErrorMap* errors) {
  if (shortcut[0] != '@') {
    return false;
  }

  errors->AddError(policy_name,
                   IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_STARTS_WITH_AT,
                   shortcut);
  return true;
}

bool ShortcutEqualsDefaultSearchProviderKeyword(const std::string& policy_name,
                                                const std::string& shortcut,
                                                const PolicyMap& policies,
                                                PolicyErrorMap* errors) {
  const base::Value* provider_enabled = policies.GetValue(
      key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  const base::Value* provider_keyword = policies.GetValue(
      key::kDefaultSearchProviderKeyword, base::Value::Type::STRING);
  // Ignore if `DefaultSearchProviderEnabled` is not set, invalid, or disabled.
  // Ignore if `DefaultSearchProviderKeyword` is not set, invalid, or different
  // from `shortcut`.
  if (!provider_enabled || !provider_enabled->GetBool() || !provider_keyword ||
      shortcut != provider_keyword->GetString()) {
    return false;
  }

  errors->AddError(policy_name,
                   IDS_POLICY_SITE_SEARCH_SETTINGS_SHORTCUT_EQUALS_DSP_KEYWORD,
                   shortcut);
  return true;
}

bool ReplacementStringIsMissingFromUrl(const std::string& policy_name,
                                       const std::string& url,
                                       PolicyErrorMap* errors) {
  TemplateURLData data;
  data.SetURL(url);
  SearchTermsData search_terms_data;
  if (TemplateURL(data).SupportsReplacement(search_terms_data)) {
    return false;
  }

  errors->AddError(
      policy_name,
      IDS_POLICY_SITE_SEARCH_SETTINGS_URL_DOESNT_SUPPORT_REPLACEMENT, url);
  return true;
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

  int num_featured = base::ranges::count_if(
      site_search_providers, [](const base::Value& provider) {
        return provider.GetDict()
            .FindBool(SiteSearchPolicyHandler::kFeatured)
            .value_or(false);
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
    const std::string& shortcut = GetShortcut(provider);
    const std::string& url = GetUrl(provider);

    // TODO(b/309457951): Add validation to ensure that at most 3 entries are
    //                    featured_by_policy.

    bool invalid_entry =
        ShortcutIsEmpty(policy_name(), shortcut, errors) ||
        NameIsEmpty(policy_name(), GetName(provider), errors) ||
        UrlIsEmpty(policy_name(), url, errors) ||
        ShortcutHasWhitespace(policy_name(), shortcut, errors) ||
        ShortcutStartsWithAtSymbol(policy_name(), shortcut, errors) ||
        ShortcutEqualsDefaultSearchProviderKeyword(policy_name(), shortcut,
                                                   policies, errors) ||
        ShortcutAlreadySeen(policy_name(), shortcut, shortcuts_already_seen,
                            errors, &duplicated_shortcuts) ||
        ReplacementStringIsMissingFromUrl(policy_name(), url, errors);

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
      const base::Value::Dict& policy_dict = item.GetDict();
      providers.Append(
          SiteSearchDictFromPolicyValue(policy_dict, /*featured=*/false));
      if (policy_dict.FindBool(SiteSearchPolicyHandler::kFeatured)
              .value_or(false)) {
        providers.Append(SiteSearchDictFromPolicyValue(policy_dict,
                                                       /*featured=*/true));
      }
    }
  }

  EnterpriseSiteSearchManager::AddPrefValueToMap(std::move(providers), prefs);
}

}  // namespace policy
