// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

std::string PermissionIDsToString(const std::set<mojom::APIPermissionID>& ids) {
  std::vector<std::string> strs;
  for (auto id : ids)
    strs.push_back(base::NumberToString(static_cast<int>(id)));
  return base::JoinString(strs, " ");
}

std::string RuleToString(const ChromePermissionMessageRule& rule) {
  return base::StringPrintf(
      "(req: %s opt: %s)",
      PermissionIDsToString(rule.required_permissions()).c_str(),
      PermissionIDsToString(rule.optional_permissions()).c_str());
}

bool MakesRedundant(const ChromePermissionMessageRule& first_rule,
                    const ChromePermissionMessageRule& second_rule) {
  // The second rule is redundant if the first rule has a (non-strict) subset
  // of its required permissions - the first rule will always "steal" those
  // permissions, so the second rule can never apply.
  // Example: Say rule 1 has required permissions A, B, and rule 2 has A, B,
  // and C. So 1 is a subset of 2. If the requirements for 2 are satisfied
  // (i.e., A, B, and C are all there), then the requirements for 1 are also
  // satisfied. Since 1 comes first, it will always take A and B, and so the
  // requirements for 2 can never be satisfied by the time it's applied.
  return base::ranges::includes(second_rule.required_permissions(),
                                first_rule.required_permissions());
}

}  // namespace

TEST(ChromePermissionMessageRulesTest, NoRedundantRules) {
  std::vector<ChromePermissionMessageRule> rules =
      ChromePermissionMessageRule::GetAllRules();
  for (size_t i = 1; i < rules.size(); i++) {
    for (size_t j = 0; j < i; j++) {
      EXPECT_FALSE(MakesRedundant(rules[j], rules[i]))
          << "Rule at index " << i << " " << RuleToString(rules[i])
          << " is redundant because of previous rule at index " << j << " "
          << RuleToString(rules[j]);
    }
  }
}

}  // namespace extensions
