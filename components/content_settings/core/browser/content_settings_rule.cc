// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_rule.h"

#include <utility>

#include "base/check.h"

namespace content_settings {

RefCountedAutoLock::RefCountedAutoLock(base::Lock& lock) : auto_lock_(lock) {}
RefCountedAutoLock::~RefCountedAutoLock() = default;

Rule::Rule(const ContentSettingsPattern& primary_pattern,
           const ContentSettingsPattern& secondary_pattern,
           const RuleMetaData& metadata)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      metadata(metadata) {}

Rule::~Rule() = default;

UnownedRule::UnownedRule(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         const base::Value* unowned_value,
                         scoped_refptr<RefCountedAutoLock> value_lock,
                         const RuleMetaData& metadata)
    : Rule(primary_pattern, secondary_pattern, metadata),
      unowned_value(unowned_value),
      value_lock(value_lock) {}

UnownedRule::~UnownedRule() = default;

const base::Value& UnownedRule::value() const {
  return *unowned_value;
}

base::Value UnownedRule::TakeValue() {
  return unowned_value->Clone();
}

OwnedRule::OwnedRule(const ContentSettingsPattern& primary_pattern,
                     const ContentSettingsPattern& secondary_pattern,
                     base::Value owned_value,
                     const RuleMetaData& metadata)
    : Rule(primary_pattern, secondary_pattern, metadata),
      owned_value(std::move(owned_value)) {}

OwnedRule::~OwnedRule() = default;

const base::Value& OwnedRule::value() const {
  return owned_value;
}

base::Value OwnedRule::TakeValue() {
  return std::move(owned_value);
}

RuleIterator::~RuleIterator() = default;

ConcatenationIterator::ConcatenationIterator(
    std::vector<std::unique_ptr<RuleIterator>> iterators)
    : iterators_(std::move(iterators)) {
  auto it = iterators_.begin();
  while (it != iterators_.end()) {
    if (!(*it)->HasNext())
      it = iterators_.erase(it);
    else
      ++it;
  }
}

ConcatenationIterator::~ConcatenationIterator() = default;

bool ConcatenationIterator::HasNext() const {
  return !iterators_.empty();
}

std::unique_ptr<Rule> ConcatenationIterator::Next() {
  auto current_iterator = iterators_.begin();
  DCHECK(current_iterator != iterators_.end());
  DCHECK((*current_iterator)->HasNext());
  std::unique_ptr<Rule> next_rule = (*current_iterator)->Next();
  if (!(*current_iterator)->HasNext()) {
    iterators_.erase(current_iterator);
  }
  return next_rule;
}

}  // namespace content_settings
