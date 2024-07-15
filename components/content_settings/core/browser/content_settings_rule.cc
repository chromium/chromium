// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_rule.h"

#include <utility>

#include "base/check.h"
#include "base/not_fatal_until.h"

namespace content_settings {

Rule::Rule(ContentSettingsPattern primary_pattern,
           ContentSettingsPattern secondary_pattern,
           base::Value value,
           RuleMetaData metadata)
    : primary_pattern(std::move(primary_pattern)),
      secondary_pattern(std::move(secondary_pattern)),
      value(std::move(value)),
      metadata(std::move(metadata)) {}

Rule::~Rule() = default;

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
  CHECK(current_iterator != iterators_.end(), base::NotFatalUntil::M130);
  DCHECK((*current_iterator)->HasNext());
  std::unique_ptr<Rule> next_rule = (*current_iterator)->Next();
  if (!(*current_iterator)->HasNext()) {
    iterators_.erase(current_iterator);
  }
  return next_rule;
}

}  // namespace content_settings
