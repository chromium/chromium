// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/safe_browsing/core/browser/db/util.h"

namespace safe_browsing {

ThreatMetadata GetThreatMetadataFromSafeBrowsingApi(
    SafeBrowsingJavaThreatType threat_type,
    const std::vector<int>& threat_attributes) {
  bool is_relevant_threat_type = false;
  SubresourceFilterType type;
  switch (threat_type) {
    case SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
      is_relevant_threat_type = true;
      type = SubresourceFilterType::ABUSIVE;
      break;
    case SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION:
      is_relevant_threat_type = true;
      type = SubresourceFilterType::BETTER_ADS;
      break;
    case SafeBrowsingJavaThreatType::NO_THREAT:
    case SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING:
    case SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE:
    case SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
    case SafeBrowsingJavaThreatType::BILLING:
      break;
  }
  if (!is_relevant_threat_type) {
    return ThreatMetadata();
  }
  // Filter level is default to ENFORCE.
  SubresourceFilterLevel level = SubresourceFilterLevel::ENFORCE;
  for (int threat_attribute : threat_attributes) {
    if (threat_attribute ==
        static_cast<int>(SafeBrowsingJavaThreatAttribute::CANARY)) {
      level = SubresourceFilterLevel::WARN;
      break;
    }
  }

  SubresourceFilterMatch subresource_filter_match;
  subresource_filter_match[type] = level;
  ThreatMetadata threat_metadata;
  threat_metadata.subresource_filter_match = subresource_filter_match;
  return threat_metadata;
}

}  // namespace safe_browsing
