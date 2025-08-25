// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/ntp_shortcuts_policy_handler.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

using ntp_tiles::EnterpriseShortcut;
using ntp_tiles::EnterpriseShortcutsStore;

namespace policy {

namespace {

bool IsNTPEnterpriseShortcutsEnabled() {
  // Check that FeatureList is available as a protection against early startup
  // crashes. Some policy providers are initialized very early even before
  // base::FeatureList is available, but when policies are finally applied, the
  // feature stack is fully initialized. The instance check ensures that the
  // final decision is delayed until all features are initialized, without any
  // other downstream effect.
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(ntp_tiles::kNtpEnterpriseShortcuts);
}

// Converts a shortcuts policy entry `policy_dict` into a dictionary to be
// saved to prefs, with fields corresponding to `EnterpriseShortcut`. `CHECK`s
// are safe since this function is only used after policy values are validated.
base::Value NTPShortcutsDictFromPolicyValue(
    const base::Value::Dict& policy_dict) {
  base::Value::Dict dict;

  // To align with `EnterpriseShortcut`, use "title" as dictionary key instead
  // of "name".
  const std::string* name =
      policy_dict.FindString(NTPShortcutsPolicyHandler::kName);
  CHECK(name);
  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyTitle, *name);

  const std::string* url =
      policy_dict.FindString(NTPShortcutsPolicyHandler::kUrl);
  CHECK(url);
  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyUrl, *url);

  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin,
           static_cast<int>(EnterpriseShortcut::PolicyOrigin::kNtpShortcuts));
  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser, false);

  const bool allow_user_edit =
      policy_dict.FindBool(NTPShortcutsPolicyHandler::kAllowUserEdit)
          .value_or(false);
  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit,
           allow_user_edit);

  const bool allow_user_delete =
      policy_dict.FindBool(NTPShortcutsPolicyHandler::kAllowUserDelete)
          .value_or(false);
  dict.Set(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete,
           allow_user_delete);

  return base::Value(std::move(dict));
}

bool UrlAlreadySeen(const std::string& policy_name,
                    const GURL& url,
                    const base::flat_set<GURL>& valid_urls_already_seen,
                    PolicyErrorMap* errors,
                    base::flat_set<GURL>* duplicated_urls) {
  if (valid_urls_already_seen.find(url) == valid_urls_already_seen.end()) {
    return false;
  }

  if (duplicated_urls->find(url) == duplicated_urls->end()) {
    duplicated_urls->insert(url);

    // Only show an warning message once per url.
    errors->AddError(policy_name, IDS_POLICY_NTP_SHORTCUTS_DUPLICATED_URL,
                     url.spec(), {}, PolicyMap::MessageType::kWarning);
  }
  return true;
}

}  // namespace

const char NTPShortcutsPolicyHandler::kName[] = "name";
const char NTPShortcutsPolicyHandler::kUrl[] = "url";
const char NTPShortcutsPolicyHandler::kAllowUserEdit[] = "allow_user_edit";
const char NTPShortcutsPolicyHandler::kAllowUserDelete[] = "allow_user_delete";

const int NTPShortcutsPolicyHandler::kMaxNtpShortcuts = 10;
const int NTPShortcutsPolicyHandler::kMaxNtpShortcutTextLength = 1000;

NTPShortcutsPolicyHandler::NTPShortcutsPolicyHandler(Schema schema)
    : SimpleSchemaValidatingPolicyHandler(
          key::kNTPShortcuts,
          ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
          schema,
          policy::SchemaOnErrorStrategy::
              SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
          SimpleSchemaValidatingPolicyHandler::
              RECOMMENDED_PROHIBITED,  // Recommended policies are not supported
                                       // since `allow_user_edit` and
                                       // `allow_user_delete` fields allow for
                                       // user modification.
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

NTPShortcutsPolicyHandler::~NTPShortcutsPolicyHandler() = default;

bool NTPShortcutsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  ignored_urls_.clear();

  if (!IsNTPEnterpriseShortcutsEnabled() || !policies.Get(policy_name())) {
    return true;
  }

  if (!SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                errors)) {
    return false;
  }

  const base::Value::List& shortcuts =
      policies.GetValue(policy_name(), base::Value::Type::LIST)->GetList();

  if (shortcuts.size() > kMaxNtpShortcuts) {
    errors->AddError(policy_name(),
                     IDS_POLICY_NTP_SHORTCUTS_MAX_SHORTCUTS_LIMIT_ERROR,
                     base::NumberToString(kMaxNtpShortcuts));
    return false;
  }

  base::flat_set<GURL> valid_urls_already_seen;
  base::flat_set<GURL> duplicated_urls;
  for (const base::Value& entry : shortcuts) {
    const base::Value::Dict& dict = entry.GetDict();
    const std::string* name = dict.FindString(kName);
    const std::string* url_str = dict.FindString(kUrl);

    // The `SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY` strategy guarantees
    // that `entry` is a dictionary, but its properties might be missing.
    // A missing name or URL is a schema validation error, which is already
    // reported as a generic warning.
    if (!url_str) {
      continue;
    }

    if (url_str->empty()) {
      errors->AddError(policy_name(), IDS_SEARCH_POLICY_SETTINGS_URL_IS_EMPTY,
                       {}, PolicyMap::MessageType::kWarning);
      continue;
    }

    GURL url(*url_str);
    if (!url.is_valid()) {
      errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR, {},
                       PolicyMap::MessageType::kWarning);
      ignored_urls_.insert(url);
      continue;
    }

    if (url_str->length() > kMaxNtpShortcutTextLength) {
      errors->AddError(policy_name(), IDS_POLICY_NTP_SHORTCUTS_URL_TOO_LONG,
                       base::NumberToString(kMaxNtpShortcutTextLength), {},
                       PolicyMap::MessageType::kWarning);
      ignored_urls_.insert(url);
      continue;
    }

    if (UrlAlreadySeen(policy_name(), url, valid_urls_already_seen, errors,
                       &duplicated_urls)) {
      ignored_urls_.insert(url);
      continue;
    }

    if (!name) {
      ignored_urls_.insert(url);
      continue;
    }

    if (name->empty()) {
      errors->AddError(policy_name(), IDS_SEARCH_POLICY_SETTINGS_NAME_IS_EMPTY,
                       {}, PolicyMap::MessageType::kWarning);
      ignored_urls_.insert(url);
      continue;
    }

    if (name->length() > kMaxNtpShortcutTextLength) {
      errors->AddError(policy_name(), IDS_POLICY_NTP_SHORTCUTS_NAME_TOO_LONG,
                       base::NumberToString(kMaxNtpShortcutTextLength), {},
                       PolicyMap::MessageType::kWarning);
      ignored_urls_.insert(url);
      continue;
    }

    valid_urls_already_seen.insert(url);
  }

  // Accept if there is at least one valid shortcut that has a unique URL
  // (`valid_urls_already_seen` stores all valid urls including duplicate URLs).
  if (valid_urls_already_seen.size() > duplicated_urls.size()) {
    return true;
  }

  errors->AddError(policy_name(), IDS_POLICY_NTP_SHORTCUTS_NO_VALID_PROVIDER);
  return false;
}

void NTPShortcutsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  // If policy handler is disabled, the pref should be cleared to prevent old
  // shortcuts from appearing.
  if (!IsNTPEnterpriseShortcutsEnabled()) {
    prefs->RemoveValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    return;
  }

  const base::Value* policy_value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!policy_value) {
    prefs->RemoveValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    return;
  }

  base::Value::List shortcuts;
  for (const base::Value& item : policy_value->GetList()) {
    const base::Value::Dict& policy_dict = item.GetDict();
    const std::string* url_str = policy_dict.FindString(kUrl);
    // An entry with a missing, empty, or invalid URL should be ignored.
    if (!url_str) {
      continue;
    }
    GURL url(*url_str);
    if (!url.is_valid()) {
      continue;
    }
    if (ignored_urls_.find(url) == ignored_urls_.end()) {
      shortcuts.Append(NTPShortcutsDictFromPolicyValue(policy_dict));
    }
  }

  prefs->SetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                  base::Value(std::move(shortcuts)));
}

}  // namespace policy
