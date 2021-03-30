// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_TEST_UTIL_H_
#define COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_TEST_UTIL_H_

#include <string>

namespace subresource_redirect {

const bool kRuleTypeAllow = true;
const bool kRuleTypeDisallow = false;

// Holds one allow or disallow robots rule
struct RobotsRule {
  RobotsRule(bool rule_type, const std::string& pattern)
      : rule_type(rule_type), pattern(pattern) {}

  bool rule_type;
  std::string pattern;
};

// Convert robots rules to its proto.
std::string GetRobotsRulesProtoString(const std::vector<RobotsRule>& patterns);

}  // namespace subresource_redirect

#endif  // COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_TEST_UTIL_H_
