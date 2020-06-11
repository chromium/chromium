// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_policy_handler.h"

#include <stddef.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {
// Extracts a list from a policy value and adds it to a pref dictionary.
void SetListInPref(const PolicyMap& policies,
                   const char* policy_name,
                   const char* key,
                   base::DictionaryValue* dict) {
  DCHECK(dict);
  const base::Value* policy_value = policies.GetValue(policy_name);
  const base::ListValue* policy_list = nullptr;
  if (policy_value) {
    bool is_list = policy_value->GetAsList(&policy_list);
    DCHECK(is_list);
  }
  dict->Set(key, policy_list
                     ? std::make_unique<base::Value>(policy_list->Clone())
                     : std::make_unique<base::Value>(base::Value::Type::LIST));
}

// Extracts a string from a policy value and adds it to a pref dictionary.
void SetStringInPref(const PolicyMap& policies,
                     const char* policy_name,
                     const char* key,
                     base::DictionaryValue* dict) {
  DCHECK(dict);
  const base::Value* policy_value = policies.GetValue(policy_name);
  std::string str;
  if (policy_value) {
    bool is_string = policy_value->GetAsString(&str);
    DCHECK(is_string);
  }
  dict->SetString(key, str);
}

void SetBooleanInPref(const PolicyMap& policies,
                      const char* policy_name,
                      const char* key,
                      base::DictionaryValue* dict) {
  DCHECK(dict);
  const base::Value* policy_value = policies.GetValue(policy_name);
  bool bool_value = false;
  if (policy_value) {
    DCHECK(policy_value->GetAsBoolean(&bool_value));
  }
  dict->SetBoolean(key, bool_value);
}

}  // namespace

// List of policy types to preference names, for policies affecting the default
// search provider.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyDataMap[] = {
    {key::kDefaultSearchProviderEnabled, prefs::kDefaultSearchProviderEnabled,
     base::Value::Type::BOOLEAN},
    {key::kDefaultSearchProviderName, DefaultSearchManager::kShortName,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderKeyword, DefaultSearchManager::kKeyword,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSearchURL, DefaultSearchManager::kURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSuggestURL,
     DefaultSearchManager::kSuggestionsURL, base::Value::Type::STRING},
    {key::kDefaultSearchProviderIconURL, DefaultSearchManager::kFaviconURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderEncodings,
     DefaultSearchManager::kInputEncodings, base::Value::Type::LIST},
    {key::kDefaultSearchProviderAlternateURLs,
     DefaultSearchManager::kAlternateURLs, base::Value::Type::LIST},
    {key::kDefaultSearchProviderImageURL, DefaultSearchManager::kImageURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderNewTabURL, DefaultSearchManager::kNewTabURL,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderSearchURLPostParams,
     DefaultSearchManager::kSearchURLPostParams, base::Value::Type::STRING},
    {key::kDefaultSearchProviderSuggestURLPostParams,
     DefaultSearchManager::kSuggestionsURLPostParams,
     base::Value::Type::STRING},
    {key::kDefaultSearchProviderImageURLPostParams,
     DefaultSearchManager::kImageURLPostParams, base::Value::Type::STRING},
    {key::kDefaultSearchProviderContextMenuAccessAllowed,
     prefs::kDefaultSearchProviderContextMenuAccessAllowed,
     base::Value::Type::BOOLEAN},
};

// DefaultSearchPolicyHandler implementation -----------------------------------

DefaultSearchPolicyHandler::DefaultSearchPolicyHandler() {}

DefaultSearchPolicyHandler::~DefaultSearchPolicyHandler() {}

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
          policy_name != key::kDefaultSearchProviderContextMenuAccessAllowed &&
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
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetBoolean(DefaultSearchManager::kDisabledByPolicy, true);
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

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (size_t i = 0; i < base::size(kDefaultSearchPolicyDataMap); ++i) {
    const char* policy_name = kDefaultSearchPolicyDataMap[i].policy_name;
    // kDefaultSearchProviderEnabled has already been handled.
    if (policy_name == key::kDefaultSearchProviderEnabled)
      continue;

    switch (kDefaultSearchPolicyDataMap[i].value_type) {
      case base::Value::Type::STRING:
        SetStringInPref(policies,
                        policy_name,
                        kDefaultSearchPolicyDataMap[i].preference_path,
                        dict.get());
        break;
      case base::Value::Type::LIST:
        SetListInPref(policies,
                      policy_name,
                      kDefaultSearchPolicyDataMap[i].preference_path,
                      dict.get());
        break;
      case base::Value::Type::BOOLEAN:
        SetBooleanInPref(policies, policy_name,
                         kDefaultSearchPolicyDataMap[i].preference_path,
                         dict.get());
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Set the fields which are not specified by the policy to default values.
  dict->SetString(DefaultSearchManager::kID,
                  base::NumberToString(kInvalidTemplateURLID));
  dict->SetInteger(DefaultSearchManager::kPrepopulateID, 0);
  dict->SetString(DefaultSearchManager::kSyncGUID, std::string());
  dict->SetString(DefaultSearchManager::kOriginatingURL, std::string());
  dict->SetBoolean(DefaultSearchManager::kSafeForAutoReplace, true);
  dict->SetDouble(DefaultSearchManager::kDateCreated,
                  base::Time::Now().ToInternalValue());
  dict->SetDouble(DefaultSearchManager::kLastModified,
                  base::Time::Now().ToInternalValue());
  dict->SetInteger(DefaultSearchManager::kUsageCount, 0);
  dict->SetBoolean(DefaultSearchManager::kCreatedByPolicy, true);

  // For the name and keyword, default to the host if not specified.  If
  // there is no host (as is the case with file URLs of the form:
  // "file:///c:/..."), use "_" to guarantee that the keyword is non-empty.
  std::string name, keyword;
  dict->GetString(DefaultSearchManager::kKeyword, &keyword);
  dict->GetString(DefaultSearchManager::kShortName, &name);
  dict->GetString(DefaultSearchManager::kURL, &url);

  std::string host(GURL(url).host());
  if (host.empty())
    host = "_";
  if (name.empty())
    dict->SetString(DefaultSearchManager::kShortName, host);
  if (keyword.empty())
    dict->SetString(DefaultSearchManager::kKeyword, host);

  DefaultSearchManager::AddPrefValueToMap(std::move(dict), prefs);
}

bool DefaultSearchPolicyHandler::CheckIndividualPolicies(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  bool all_ok = true;
  for (const auto& policy_map_entry : kDefaultSearchPolicyDataMap) {
    // It's important to check policy type for all policies and not just exit on
    // the first error, so we report all policy errors.
    const base::Value* value = policies.GetValue(policy_map_entry.policy_name);
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
  const base::Value* provider_enabled =
      policies.GetValue(key::kDefaultSearchProviderEnabled);
  bool enabled = true;
  return provider_enabled && provider_enabled->GetAsBoolean(&enabled) &&
      !enabled;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderPolicyIsSet(
    const PolicyMap& policies) {
  return HasDefaultSearchPolicy(policies, key::kDefaultSearchProviderEnabled);
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies,
    const base::Value** url_value,
    std::string* url_string) {
  *url_value = policies.GetValue(key::kDefaultSearchProviderSearchURL);
  if (!*url_value || !(*url_value)->GetAsString(url_string) ||
      url_string->empty())
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
  base::ListValue* list_value;
  if (!prefs->GetValue(path, &value) || !value->GetAsList(&list_value))
    prefs->SetValue(path, base::Value(base::Value::Type::LIST));
}

}  // namespace policy
