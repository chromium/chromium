// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/search_engine_fields_validators.h"

#include "base/strings/string_util.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy::search_engine_fields_validators {

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

}  // namespace policy::search_engine_fields_validators
