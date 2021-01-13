// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_TEST_UTIL_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_TEST_UTIL_H_

#include <string>

namespace subresource_redirect {

const bool kRuleTypeAllow = true;
const bool kRuleTypeDisallow = false;

struct Rule {
  Rule(bool rule_type, std::string pattern)
      : rule_type(rule_type), pattern(pattern) {}

  bool rule_type;
  std::string pattern;
};

std::string GetRobotsRulesProtoString(const std::vector<Rule>& patterns);

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_TEST_UTIL_H_
