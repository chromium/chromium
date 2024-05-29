// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_RULES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_RULES_H_

#include <map>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

// Shared definitions for OriginValueMap and HostIndexedContentSettings to store
// a set of ContentSetting rules in order of precedence.

struct SortedPatternPair {
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;

  SortedPatternPair(const ContentSettingsPattern& primary_pattern,
                    const ContentSettingsPattern& secondary_pattern);
  bool operator<(const SortedPatternPair& other) const;
};

struct ValueEntry {
  base::Value value;
  RuleMetaData metadata;
  ValueEntry();
  ~ValueEntry();

  ValueEntry(ValueEntry&& other);
  ValueEntry& operator=(ValueEntry&& other);
};

typedef std::pair<const SortedPatternPair, ValueEntry> RuleEntry;
typedef std::map<SortedPatternPair, ValueEntry> Rules;

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_RULES_H_
