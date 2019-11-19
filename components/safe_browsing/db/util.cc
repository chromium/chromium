// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/util.h"

#include <stddef.h>

namespace safe_browsing {

ThreatMetadata::ThreatMetadata()
    : threat_pattern_type(ThreatPatternType::NONE) {}

ThreatMetadata::ThreatMetadata(const ThreatMetadata& other) = default;

ThreatMetadata::~ThreatMetadata() {}

bool ThreatMetadata::operator==(const ThreatMetadata& other) const {
  return threat_pattern_type == other.threat_pattern_type &&
         api_permissions == other.api_permissions &&
         subresource_filter_match == other.subresource_filter_match &&
         population_id == other.population_id;
}

bool ThreatMetadata::operator!=(const ThreatMetadata& other) const {
  return !operator==(other);
}

std::unique_ptr<base::trace_event::TracedValue> ThreatMetadata::ToTracedValue()
    const {
  auto value = std::make_unique<base::trace_event::TracedValue>();

  value->SetInteger("threat_pattern_type",
                    static_cast<int>(threat_pattern_type));

  value->BeginArray("api_permissions");
  for (const std::string& permission : api_permissions) {
    value->AppendString(permission);
  }
  value->EndArray();

  value->BeginDictionary("subresource_filter_match");
  for (const auto& it : subresource_filter_match) {
    value->BeginArray("match_metadata");
    value->AppendInteger(static_cast<int>(it.first));
    value->AppendInteger(static_cast<int>(it.second));
    value->EndArray();
  }
  value->EndDictionary();

  value->SetString("popuplation_id", population_id);
  return value;
}

}  // namespace safe_browsing
