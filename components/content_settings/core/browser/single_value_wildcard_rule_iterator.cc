// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/single_value_wildcard_rule_iterator.h"

namespace content_settings {

SingleValueWildcardRuleIterator::SingleValueWildcardRuleIterator(
    base::Value value) {
  if (!value.is_none()) {
    value_ = std::move(value);
  } else {
    is_done_ = true;
  }
}

SingleValueWildcardRuleIterator::~SingleValueWildcardRuleIterator() = default;

bool SingleValueWildcardRuleIterator::HasNext() const {
  return !is_done_;
}

std::unique_ptr<Rule> SingleValueWildcardRuleIterator::Next() {
  CHECK(HasNext());
  is_done_ = true;
  return std::make_unique<Rule>(ContentSettingsPattern::Wildcard(),
                                ContentSettingsPattern::Wildcard(),
                                std::move(value_), RuleMetaData{});
}

}  // namespace content_settings
