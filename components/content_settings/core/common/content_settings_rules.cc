// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_rules.h"

namespace content_settings {

SortedPatternPair::SortedPatternPair(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern)
    : primary_pattern(primary_pattern), secondary_pattern(secondary_pattern) {}

bool SortedPatternPair::operator<(const SortedPatternPair& other) const {
  // Note that this operator is the other way around than
  // |ContentSettingsPattern::operator<|. It sorts patterns with higher
  // precedence first.
  return std::tie(primary_pattern, secondary_pattern) >
         std::tie(other.primary_pattern, other.secondary_pattern);
}

ValueEntry::ValueEntry() = default;

ValueEntry::~ValueEntry() = default;

ValueEntry::ValueEntry(ValueEntry&& other) = default;

ValueEntry& ValueEntry::operator=(ValueEntry&& other) = default;

}  // namespace content_settings
