// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_UTILS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_UTILS_H_

#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"

namespace feature_engagement {

struct BlockedBy;
struct Blocking;
struct Comparator;
struct EventConfig;
struct SessionRateImpact;
struct SnoozeParams;

// Holds all the possible fields that can be parsed. The parsing code will fill
// the provided items with parsed data. If any field is null, then it won't be
// parsed.
struct ConfigParseOutput {
  const base::raw_ref<uint32_t> parse_errors;
  raw_ptr<Comparator> session_rate = nullptr;
  raw_ptr<SessionRateImpact> session_rate_impact = nullptr;
  raw_ptr<Blocking> blocking = nullptr;
  raw_ptr<BlockedBy> blocked_by = nullptr;
  raw_ptr<EventConfig> trigger = nullptr;
  raw_ptr<EventConfig> used = nullptr;
  raw_ptr<std::set<EventConfig>> event_configs = nullptr;
  raw_ptr<bool> tracking_only = nullptr;
  raw_ptr<Comparator> availability = nullptr;
  raw_ptr<SnoozeParams> snooze_params = nullptr;
  raw_ptr<std::vector<std::string>> groups = nullptr;

  explicit ConfigParseOutput(uint32_t& parse_errors);
};

void ParseConfigFields(const base::Feature* feature,
                       std::map<std::string, std::string> params,
                       ConfigParseOutput& output,
                       const FeatureVector& known_features,
                       const GroupVector& known_groups);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_UTILS_H_
