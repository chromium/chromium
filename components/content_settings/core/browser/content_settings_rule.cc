// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_rule.h"

#include <utility>

#include "base/check.h"

namespace content_settings {

Rule::Rule() = default;

Rule::Rule(const ContentSettingsPattern& primary_pattern,
           const ContentSettingsPattern& secondary_pattern,
           base::Value value,
           base::Time expiration,
           SessionModel session_model)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      value(std::move(value)),
      expiration(expiration),
      session_model(session_model) {}

Rule::Rule(Rule&& other) = default;

Rule& Rule::operator=(Rule&& other) = default;

Rule::~Rule() = default;

RuleIterator::~RuleIterator() = default;

ConcatenationIterator::ConcatenationIterator(
    std::vector<std::unique_ptr<RuleIterator>> iterators,
    base::AutoLock* auto_lock)
    : iterators_(std::move(iterators)), auto_lock_(auto_lock) {
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

Rule ConcatenationIterator::Next() {
  auto current_iterator = iterators_.begin();
  DCHECK(current_iterator != iterators_.end());
  DCHECK((*current_iterator)->HasNext());
  const Rule& next_rule = (*current_iterator)->Next();
  Rule to_return(next_rule.primary_pattern, next_rule.secondary_pattern,
                 next_rule.value.Clone(), next_rule.expiration,
                 next_rule.session_model);
  if (!(*current_iterator)->HasNext())
    iterators_.erase(current_iterator);
  return to_return;
}

}  // namespace content_settings
