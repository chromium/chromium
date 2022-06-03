// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_redirect/subresource_redirect_test_util.h"

#include "components/subresource_redirect/proto/robots_rules.pb.h"

namespace subresource_redirect {

std::string GetRobotsRulesProtoString(const std::vector<RobotsRule>& patterns) {
  proto::RobotsRules robots_rules;
  for (const auto& pattern : patterns) {
    auto* new_rule = robots_rules.add_image_ordered_rules();
    if (pattern.rule_type == kRuleTypeAllow) {
      new_rule->set_allowed_pattern(pattern.pattern);
    } else {
      new_rule->set_disallowed_pattern(pattern.pattern);
    }
  }
  return robots_rules.SerializeAsString();
}

}  // namespace subresource_redirect
