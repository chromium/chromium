// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_ENGINE_FIELDS_VALIDATORS_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_ENGINE_FIELDS_VALIDATORS_H_

#include <string>

#include "base/values.h"

namespace policy {

class PolicyErrorMap;
class PolicyMap;

namespace search_engine_fields_validators {

bool ShortcutIsEmpty(const std::string& policy_name,
                     const std::string& shortcut,
                     PolicyErrorMap* errors);

bool NameIsEmpty(const std::string& policy_name,
                 const std::string& name,
                 PolicyErrorMap* errors);

bool UrlIsEmpty(const std::string& policy_name,
                const std::string& url,
                PolicyErrorMap* errors);

bool ShortcutHasWhitespace(const std::string& policy_name,
                           const std::string& shortcut,
                           PolicyErrorMap* errors);

bool ShortcutStartsWithAtSymbol(const std::string& policy_name,
                                const std::string& shortcut,
                                PolicyErrorMap* errors);

bool ShortcutEqualsDefaultSearchProviderKeyword(const std::string& policy_name,
                                                const std::string& shortcut,
                                                const PolicyMap& policies,
                                                PolicyErrorMap* errors);

bool ReplacementStringIsMissingFromUrl(const std::string& policy_name,
                                       const std::string& url,
                                       PolicyErrorMap* errors);

}  // namespace search_engine_fields_validators
}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_SEARCH_ENGINE_FIELDS_VALIDATORS_H_
