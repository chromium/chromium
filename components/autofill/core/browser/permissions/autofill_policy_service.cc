// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_policy_service.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace autofill {

namespace {

bool IsCategoryGloballyBlocked(
    const PrefService& prefs,
    AutofillClient::AutofillPolicyDataCategory category) {
  // Global / Legacy Policy Layer: Check if the category is disabled globally
  // (either by the user's settings toggles, or by legacy enterprise policies
  // like AutofillAddressEnabled / AutofillCreditCardEnabled). If so, it is
  // blocked for all URLs immediately.
  switch (category) {
    case AutofillClient::AutofillPolicyDataCategory::kContactInfo:
      if (!prefs::IsAutofillProfileEnabled(&prefs)) {
        return true;
      }
      break;
    case AutofillClient::AutofillPolicyDataCategory::kPayments:
      if (!prefs::IsAutofillPaymentMethodsEnabled(&prefs)) {
        return true;
      }
      break;
    case AutofillClient::AutofillPolicyDataCategory::kIdentityDocs:
      if (!prefs.GetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled)) {
        return true;
      }
      break;
    case AutofillClient::AutofillPolicyDataCategory::kTravel:
      if (!prefs.GetBoolean(prefs::kAutofillAiTravelEntitiesEnabled)) {
        return true;
      }
      break;
    case AutofillClient::AutofillPolicyDataCategory::kShopping:
      if (!prefs.GetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled)) {
        return true;
      }
      break;
  }
  return false;
}

struct ParsedPolicyEntry {
  ContentSettingsPattern pattern;
  std::vector<AutofillClient::AutofillPolicyDataCategory> categories;
};

// Parses a base::Value entry from the `kAutofillTypesBlocked` pref into a
// ParsedPolicyEntry. The schema of `entry` is expected to be a dict:
// {
//   "url_pattern": "<content settings pattern string>",
//   "blocked_types": ["contact_info", "payments", ...]
// }
// Returns std::nullopt if the entry doesn't match the schema or is invalid.
std::optional<ParsedPolicyEntry> ParsePolicyEntry(const base::Value& entry) {
  if (!entry.is_dict()) {
    return std::nullopt;
  }

  const base::DictValue& entry_dict = entry.GetDict();
  const std::string* pattern_str =
      entry_dict.FindString(prefs::kAutofillBlockedTypesUrlPatternKey);
  const base::ListValue* blocked_types =
      entry_dict.FindList(prefs::kAutofillBlockedTypesBlockedTypesKey);

  if (!pattern_str || !blocked_types) {
    return std::nullopt;
  }

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(*pattern_str);
  if (!pattern.IsValid()) {
    return std::nullopt;
  }

  std::vector<AutofillClient::AutofillPolicyDataCategory> categories;
  for (const base::Value& blocked_type : *blocked_types) {
    if (!blocked_type.is_string()) {
      continue;
    }
    std::string_view type_str = blocked_type.GetString();
    if (type_str == prefs::kAutofillBlockedTypesContactInfoValue) {
      categories.push_back(
          AutofillClient::AutofillPolicyDataCategory::kContactInfo);
    } else if (type_str == prefs::kAutofillBlockedTypesPaymentsValue) {
      categories.push_back(
          AutofillClient::AutofillPolicyDataCategory::kPayments);
    } else if (type_str == prefs::kAutofillBlockedTypesIdentityDocsValue) {
      categories.push_back(
          AutofillClient::AutofillPolicyDataCategory::kIdentityDocs);
    } else if (type_str == prefs::kAutofillBlockedTypesTravelValue) {
      categories.push_back(AutofillClient::AutofillPolicyDataCategory::kTravel);
    } else if (type_str == prefs::kAutofillBlockedTypesShoppingValue) {
      categories.push_back(
          AutofillClient::AutofillPolicyDataCategory::kShopping);
    } else if (type_str == prefs::kAutofillBlockedTypesAllValue) {
      // LINT.IfChange(AutofillPolicyDataCategory)
      categories.insert(
          categories.end(),
          {AutofillClient::AutofillPolicyDataCategory::kContactInfo,
           AutofillClient::AutofillPolicyDataCategory::kPayments,
           AutofillClient::AutofillPolicyDataCategory::kIdentityDocs,
           AutofillClient::AutofillPolicyDataCategory::kTravel,
           AutofillClient::AutofillPolicyDataCategory::kShopping});
      // LINT.ThenChange(//components/autofill/core/browser/foundations/autofill_client.h:AutofillPolicyDataCategory,//components/autofill/core/browser/permissions/autofill_policy_service_unittest.cc:AutofillPolicyDataCategory)
    }
  }
  return ParsedPolicyEntry{std::move(pattern), std::move(categories)};
}

// Evaluates the given `policy_list` (from the `kAutofillTypesBlocked` pref)
// to check whether the given `category` is blocked for the specified `url`.
// If `url` is empty, this checks if the category is blocked globally via a
// wildcard pattern ("*").
bool EvaluatePolicyList(const base::ListValue& policy_list,
                        const GURL& url,
                        AutofillClient::AutofillPolicyDataCategory category) {
  return std::ranges::any_of(policy_list, [&](const base::Value& entry) {
    std::optional<ParsedPolicyEntry> parsed_entry = ParsePolicyEntry(entry);
    if (!parsed_entry) {
      return false;
    }

    bool matches_url =
        url.is_empty()
            ? (parsed_entry->pattern == ContentSettingsPattern::Wildcard())
            : parsed_entry->pattern.Matches(url);

    return matches_url &&
           std::ranges::contains(parsed_entry->categories, category);
  });
}

}  // namespace

AutofillPolicyService::AutofillPolicyService(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    autofill_types_blocked_change_registrar_.Init(prefs);
    autofill_types_blocked_change_registrar_.Add(
        prefs::kAutofillTypesBlocked,
        base::BindRepeating(&AutofillPolicyService::OnAutofillPolicyChanged,
                            base::Unretained(this)));
    OnAutofillPolicyChanged();
  }
}

AutofillPolicyService::~AutofillPolicyService() = default;

// static
bool AutofillPolicyService::IsAutofillTypeBlockedByPolicyFromPref(
    const PrefService& prefs,
    const GURL& url,
    AutofillClient::AutofillPolicyDataCategory category) {
  if (IsCategoryGloballyBlocked(prefs, category)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    return false;
  }

  const base::ListValue& policy_list =
      prefs.GetList(prefs::kAutofillTypesBlocked);
  return EvaluatePolicyList(policy_list, url, category);
}

bool AutofillPolicyService::IsAutofillTypeBlockedByPolicy(
    const GURL& url,
    AutofillClient::AutofillPolicyDataCategory category) const {
  if (IsCategoryGloballyBlocked(*prefs_, category)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    return false;
  }

  // Enterprise Policy Layer: Check if there is an active GPO domain blocking
  // rule that matches the navigation URL for the requested data category.
  return std::ranges::any_of(
      blocked_patterns_cache_, [&](const BlockedPatternEntry& entry) {
        return entry.pattern.Matches(url) &&
               std::ranges::contains(entry.blocked_categories, category);
      });
}

void AutofillPolicyService::OnAutofillPolicyChanged() {
  blocked_patterns_cache_.clear();
  const base::ListValue& blocked_list =
      prefs_->GetList(prefs::kAutofillTypesBlocked);
  for (const base::Value& entry : blocked_list) {
    if (std::optional<ParsedPolicyEntry> parsed_entry =
            ParsePolicyEntry(entry)) {
      blocked_patterns_cache_.push_back({std::move(parsed_entry->pattern),
                                         std::move(parsed_entry->categories)});
    }
  }
}

}  // namespace autofill
