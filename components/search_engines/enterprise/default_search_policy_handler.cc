// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/default_search_policy_handler.h"

#include <stddef.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {
// Extracts a list from a policy value and adds it to a pref dictionary.
void SetListInPref(const PolicyMap& policies,
                   const char* policy_name,
                   const char* key,
                   base::Value::Dict& dict) {
  const base::Value* policy_value =
      policies.GetValue(policy_name, base::Value::Type::LIST);
  dict.Set(key, policy_value ? policy_value->Clone()
                             : base::Value(base::Value::Type::LIST));
}

// Extracts a string from a policy value and adds it to a pref dictionary.
void SetStringInPref(const PolicyMap& policies,
                     const char* policy_name,
                     const char* key,
                     base::Value::Dict& dict) {
  const base::Value* policy_value =
      policies.GetValue(policy_name, base::Value::Type::STRING);
  dict.Set(key, policy_value ? policy_value->GetString() : std::string());
}

void SetBooleanInPref(const PolicyMap& policies,
                      const char* policy_name,
                      const char* key,
                      base::Value::Dict& dict) {
  const base::Value* policy_value =
      policies.GetValue(policy_name, base::Value::Type::BOOLEAN);
  dict.SetByDottedPath(key, policy_value && policy_value->GetBool());
}

}  // namespace

// List of policy types to preference names, for policies affecting the default
// search provider. Please update ApplyPolicySettings() when add or remove
// items.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyDataMap[] = {
    {key::kDefaultSearchProviderEnabled, prefs::kDefaultSearchProviderEnabled,
     base::Value::Type::BOOLEAN},
    {key::kDefaultSearchProviderName, DefaultSearchManager::kShortName,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSearchURL, DefaultSearchManager::kURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSuggestURL,
     DefaultSearchManager::kSuggestionsURL, base::Value::Type::STRING},
    {key::kDefaultSearchProviderEncodings,
     DefaultSearchManager::kInputEncodings, base::Value::Type::LIST},
    {key::kDefaultSearchProviderAlternateURLs,
     DefaultSearchManager::kAlternateURLs, base::Value::Type::LIST},
    {key::kDefaultSearchProviderImageURL, DefaultSearchManager::kImageURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSearchURLPostParams,
     DefaultSearchManager::kSearchURLPostParams, base::Value::Type::STRING},
    {key::kDefaultSearchProviderSuggestURLPostParams,
     DefaultSearchManager::kSuggestionsURLPostParams,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderImageURLPostParams,
     DefaultSearchManager::kImageURLPostParams, base::Value::Type::STRING},
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    {key::kDefaultSearchProviderContextMenuAccessAllowed,
     prefs::kDefaultSearchProviderContextMenuAccessAllowed,
     base::Value::Type::BOOLEAN},
    {key::kDefaultSearchProviderKeyword, DefaultSearchManager::kKeyword,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderNewTabURL, DefaultSearchManager::kNewTabURL,
     base::Value::Type::STRING},
#endif
};

// DefaultSearchPolicyHandler implementation -----------------------------------

DefaultSearchPolicyHandler::DefaultSearchPolicyHandler() = default;

DefaultSearchPolicyHandler::~DefaultSearchPolicyHandler() = default;

bool DefaultSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  if (!CheckIndividualPolicies(policies, errors))
    return false;

  if (!DefaultSearchProviderPolicyIsSet(policies) ||
      DefaultSearchProviderIsDisabled(policies)) {
    // Add an error for all specified default search policies except
    // DefaultSearchProviderEnabled and
    // DefaultSearchProviderContextMenuAccessAllowed.

    for (const auto& policy_map_entry : kDefaultSearchPolicyDataMap) {
      const char* policy_name = policy_map_entry.policy_name;
      if (policy_name != key::kDefaultSearchProviderEnabled &&
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
          policy_name != key::kDefaultSearchProviderContextMenuAccessAllowed &&
#endif
          HasDefaultSearchPolicy(policies, policy_name)) {
        errors->AddError(policy_name, IDS_POLICY_DEFAULT_SEARCH_DISABLED);
      }
    }
    return true;
  }

  const base::Value* url;
  std::string dummy;
  if (DefaultSearchURLIsValid(policies, &url, &dummy) ||
      !AnyDefaultSearchPoliciesSpecified(policies))
    return true;
  errors->AddError(key::kDefaultSearchProviderSearchURL, url ?
      IDS_POLICY_INVALID_SEARCH_URL_ERROR : IDS_POLICY_NOT_SPECIFIED_ERROR);
  return false;
}

void DefaultSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  // If the main switch is not set don't set anything.
  if (!DefaultSearchProviderPolicyIsSet(policies))
    return;

  if (DefaultSearchProviderIsDisabled(policies)) {
    base::Value::Dict dict;
    dict.Set(DefaultSearchManager::kDisabledByPolicy, true);
    DefaultSearchManager::AddPrefValueToMap(std::move(dict), prefs);
    return;
  }

  // The search URL is required.  The other entries are optional.  Just make
  // sure that they are all specified via policy, so that the regular prefs
  // aren't used.
  const base::Value* dummy;
  std::string url;
  if (!DefaultSearchURLIsValid(policies, &dummy, &url))
    return;

  base::Value::Dict dict;

  // Set pref values for policies affecting the default
  // search provider, which are listed in kDefaultSearchPolicyDataMap.
  // Set or remove pref accordingly when kDefaultSearchPolicyDataMap has a
  // change, then revise the number in the check below to be correct.
  SetBooleanInPref(policies, key::kDefaultSearchProviderEnabled,
                   prefs::kDefaultSearchProviderEnabled, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderName,
                  DefaultSearchManager::kShortName, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderSearchURL,
                  DefaultSearchManager::kURL, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderSuggestURL,
                  DefaultSearchManager::kSuggestionsURL, dict);
  SetListInPref(policies, key::kDefaultSearchProviderEncodings,
                DefaultSearchManager::kInputEncodings, dict);
  SetListInPref(policies, key::kDefaultSearchProviderAlternateURLs,
                DefaultSearchManager::kAlternateURLs, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderImageURL,
                  DefaultSearchManager::kImageURL, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderSearchURLPostParams,
                  DefaultSearchManager::kSearchURLPostParams, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderSuggestURLPostParams,
                  DefaultSearchManager::kSuggestionsURLPostParams, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderImageURLPostParams,
                  DefaultSearchManager::kImageURLPostParams, dict);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  SetBooleanInPref(policies,
                   key::kDefaultSearchProviderContextMenuAccessAllowed,
                   prefs::kDefaultSearchProviderContextMenuAccessAllowed, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderKeyword,
                  DefaultSearchManager::kKeyword, dict);
  SetStringInPref(policies, key::kDefaultSearchProviderNewTabURL,
                  DefaultSearchManager::kNewTabURL, dict);
  size_t policyCount = 13;
#else
  size_t policyCount = 10;
#endif

  CHECK_EQ(policyCount, std::size(kDefaultSearchPolicyDataMap));

  // Set the fields which are not specified by the policy to default values.
  dict.Set(DefaultSearchManager::kID,
           base::NumberToString(kInvalidTemplateURLID));
  dict.Set(DefaultSearchManager::kPrepopulateID, 0);
  dict.Set(DefaultSearchManager::kStarterPackId, 0);
  dict.Set(DefaultSearchManager::kSyncGUID, std::string());
  dict.Set(DefaultSearchManager::kOriginatingURL, std::string());
  dict.Set(DefaultSearchManager::kSafeForAutoReplace, true);
  dict.Set(DefaultSearchManager::kDateCreated,
           static_cast<double>(base::Time::Now().ToInternalValue()));
  dict.Set(DefaultSearchManager::kLastModified,
           static_cast<double>(base::Time::Now().ToInternalValue()));
  dict.Set(DefaultSearchManager::kUsageCount, 0);
  dict.Set(DefaultSearchManager::kCreatedByPolicy,
           static_cast<int>(
               TemplateURLData::CreatedByPolicy::kDefaultSearchProvider));

  // For the name and keyword, default to the host if not specified.  If
  // there is no host (as is the case with file URLs of the form:
  // "file:///c:/..."), use "_" to guarantee that the keyword is non-empty.
  std::string* keyword = dict.FindString(DefaultSearchManager::kKeyword);
  std::string* name = dict.FindString(DefaultSearchManager::kShortName);
  std::string* url_str = dict.FindString(DefaultSearchManager::kURL);
  if (url_str)
    url = *url_str;

  std::string host(GURL(url).host());
  if (host.empty())
    host = "_";
  if (!name || name->empty())
    dict.Set(DefaultSearchManager::kShortName, host);
  if (!keyword || keyword->empty())
    dict.Set(DefaultSearchManager::kKeyword, host);

  DefaultSearchManager::AddPrefValueToMap(std::move(dict), prefs);
}

bool DefaultSearchPolicyHandler::CheckIndividualPolicies(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  bool all_ok = true;
  for (const auto& policy_map_entry : kDefaultSearchPolicyDataMap) {
    // It's safe to use `GetValueUnsafe()` as multiple policy types are handled.
    // It's important to check policy type for all policies and not just exit on
    // the first error, so we report all policy errors.
    const base::Value* value =
        policies.GetValueUnsafe(policy_map_entry.policy_name);
    if (value && value->type() != policy_map_entry.value_type) {
      errors->AddError(policy_map_entry.policy_name, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(policy_map_entry.value_type));
      all_ok = false;
    }
  }
  return all_ok;
}

bool DefaultSearchPolicyHandler::HasDefaultSearchPolicy(
    const PolicyMap& policies,
    const char* policy_name) {
  return policies.Get(policy_name) != nullptr;
}

bool DefaultSearchPolicyHandler::AnyDefaultSearchPoliciesSpecified(
    const PolicyMap& policies) {
  for (const auto& policy_map_entry : kDefaultSearchPolicyDataMap) {
    if (policies.Get(policy_map_entry.policy_name))
      return true;
  }
  return false;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderIsDisabled(
    const PolicyMap& policies) {
  const base::Value* provider_enabled = policies.GetValue(
      key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  return provider_enabled && !provider_enabled->GetBool();
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderPolicyIsSet(
    const PolicyMap& policies) {
  return HasDefaultSearchPolicy(policies, key::kDefaultSearchProviderEnabled);
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies,
    const base::Value** url_value,
    std::string* url_string) {
  *url_value = policies.GetValue(key::kDefaultSearchProviderSearchURL,
                                 base::Value::Type::STRING);
  if (!*url_value)
    return false;

  *url_string = (*url_value)->GetString();
  if (url_string->empty())
    return false;
  TemplateURLData data;
  data.SetURL(*url_string);
  SearchTermsData search_terms_data;
  return TemplateURL(data).SupportsReplacement(search_terms_data);
}

void DefaultSearchPolicyHandler::EnsureStringPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  std::string value;
  if (!prefs->GetString(path, &value))
    prefs->SetString(path, value);
}

void DefaultSearchPolicyHandler::EnsureListPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  base::Value* value;
  if (!prefs->GetValue(path, &value) || !value->is_list())
    prefs->SetValue(path, base::Value(base::Value::Type::LIST));
}

}  // namespace policy
