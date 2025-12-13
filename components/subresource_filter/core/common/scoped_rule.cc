// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/scoped_rule.h"

#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/url_rule_util.h"

namespace subresource_filter {

ScopedRule::ScopedRule() = default;

ScopedRule::ScopedRule(scoped_refptr<const MemoryMappedRuleset> ruleset,
                       const url_pattern_index::flat::UrlRule* rule)
    : ruleset_(ruleset), rule_(rule) {
  CHECK(ruleset_);
  CHECK(rule_);
}

ScopedRule::ScopedRule(ScopedRule&& other) = default;
ScopedRule& ScopedRule::operator=(ScopedRule&& other) = default;

ScopedRule::ScopedRule(const ScopedRule& other) = default;
ScopedRule& ScopedRule::operator=(const ScopedRule& other) = default;

ScopedRule::~ScopedRule() = default;

bool ScopedRule::IsValid() const {
  return rule_;
}

std::string ScopedRule::ToString() const {
  CHECK(IsValid());
  return url_pattern_index::FlatUrlRuleToFilterlistString(rule_);
}

}  // namespace subresource_filter
