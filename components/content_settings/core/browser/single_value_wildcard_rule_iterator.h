// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_SINGLE_VALUE_WILDCARD_RULE_ITERATOR_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_SINGLE_VALUE_WILDCARD_RULE_ITERATOR_H_

#include "components/content_settings/core/browser/content_settings_rule.h"

namespace content_settings {

// RuleIterator which returns either zero or one value.
class SingleValueWildcardRuleIterator : public RuleIterator {
 public:
  explicit SingleValueWildcardRuleIterator(base::Value value);
  ~SingleValueWildcardRuleIterator() override;

  SingleValueWildcardRuleIterator(const SingleValueWildcardRuleIterator&) =
      delete;
  SingleValueWildcardRuleIterator& operator=(
      const SingleValueWildcardRuleIterator&) = delete;

  bool HasNext() const override;
  std::unique_ptr<Rule> Next() override;

 private:
  bool is_done_ = false;
  base::Value value_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_SINGLE_VALUE_WILDCARD_RULE_ITERATOR_H_
