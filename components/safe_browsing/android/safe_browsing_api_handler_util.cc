// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/safe_browsing/core/browser/db/metadata.pb.h"
#include "components/safe_browsing/core/browser/db/util.h"

namespace safe_browsing {
namespace {

// JSON metatdata keys. These are are fixed in the Java-side API.
const char kJsonKeyMatches[] = "matches";
const char kJsonKeyThreatType[] = "threat_type";

// Parse the optional "UserPopulation" key from the metadata.
// Returns empty string if none was found.
std::string ParseUserPopulation(const base::Value::Dict& match) {
  const std::string* population_id = match.FindString("UserPopulation");
  if (!population_id)
    return std::string();
  else
    return *population_id;
}

SubresourceFilterMatch ParseSubresourceFilterMatch(
    const base::Value::Dict& match) {
  SubresourceFilterMatch subresource_filter_match;

  auto get_enforcement = [](const std::string& value) {
    return value == "warn" ? SubresourceFilterLevel::WARN
                           : SubresourceFilterLevel::ENFORCE;
  };
  const std::string* absv_value = match.FindString("sf_absv");
  if (absv_value) {
    subresource_filter_match[SubresourceFilterType::ABUSIVE] =
        get_enforcement(*absv_value);
  }
  const std::string* bas_value = match.FindString("sf_bas");
  if (bas_value) {
    subresource_filter_match[SubresourceFilterType::BETTER_ADS] =
        get_enforcement(*bas_value);
  }
  return subresource_filter_match;
}

// Returns the severity level for a given SafeBrowsing list. The lowest value is
// 0, which represents the most severe list.
int GetThreatSeverity(SafetyNetJavaThreatType threat_type) {
  switch (threat_type) {
    case SafetyNetJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      return 0;
    case SafetyNetJavaThreatType::SOCIAL_ENGINEERING:
      return 1;
    case SafetyNetJavaThreatType::UNWANTED_SOFTWARE:
      return 2;
    case SafetyNetJavaThreatType::SUBRESOURCE_FILTER:
      return 3;
    case SafetyNetJavaThreatType::BILLING:
      return 4;
    case SafetyNetJavaThreatType::CSD_ALLOWLIST:
      return 5;
    case SafetyNetJavaThreatType::MAX_VALUE:
      return std::numeric_limits<int>::max();
  }
  NOTREACHED() << "Unhandled threat_type: " << static_cast<int>(threat_type);
  return std::numeric_limits<int>::max();
}

SBThreatType SafetyNetJavaToSBThreatType(
    SafetyNetJavaThreatType java_threat_num) {
  switch (java_threat_num) {
    case SafetyNetJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case SafetyNetJavaThreatType::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case SafetyNetJavaThreatType::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case SafetyNetJavaThreatType::SUBRESOURCE_FILTER:
      return SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    case SafetyNetJavaThreatType::BILLING:
      return SB_THREAT_TYPE_BILLING;
    default:
      // Unknown threat type
      return SB_THREAT_TYPE_SAFE;
  }
}

}  // namespace

// Valid examples:
// {"matches":[{"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4"},
//             {"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4", "UserPopulation":"YXNvZWZpbmFqO..."}]
UmaRemoteCallResult ParseJsonFromGMSCore(const std::string& metadata_str,
                                         SBThreatType* worst_sb_threat_type,
                                         ThreatMetadata* metadata) {
  *worst_sb_threat_type = SB_THREAT_TYPE_SAFE;  // Default to safe.
  *metadata = ThreatMetadata();                 // Default values.

  if (metadata_str.empty())
    return UmaRemoteCallResult::JSON_EMPTY;

  // Pick out the "matches" list.
  absl::optional<base::Value> value = base::JSONReader::Read(metadata_str);
  const base::Value::List* matches = nullptr;
  {
    if (!value.has_value())
      return UmaRemoteCallResult::JSON_FAILED_TO_PARSE;

    base::Value::Dict* dict = value->GetIfDict();
    if (!dict)
      return UmaRemoteCallResult::JSON_FAILED_TO_PARSE;

    matches = dict->FindList(kJsonKeyMatches);
    if (!matches)
      return UmaRemoteCallResult::JSON_FAILED_TO_PARSE;
  }

  // Go through each matched threat type and pick the most severe.
  SafetyNetJavaThreatType worst_threat_type =
      SafetyNetJavaThreatType::MAX_VALUE;
  const base::Value::Dict* worst_match = nullptr;
  for (const base::Value& match_value : *matches) {
    const base::Value::Dict* match = match_value.GetIfDict();
    if (!match) {
      continue;  // Skip malformed list entries.
    }

    // Get the threat number
    const std::string* threat_num_str = match->FindString(kJsonKeyThreatType);
    int threat_type_num;
    if (!threat_num_str ||
        !base::StringToInt(*threat_num_str, &threat_type_num)) {
      continue;  // Skip malformed list entries.
    }

    SafetyNetJavaThreatType threat_type =
        static_cast<SafetyNetJavaThreatType>(threat_type_num);
    if (threat_type > SafetyNetJavaThreatType::MAX_VALUE) {
      threat_type = SafetyNetJavaThreatType::MAX_VALUE;
    }
    if (GetThreatSeverity(threat_type) < GetThreatSeverity(worst_threat_type)) {
      worst_threat_type = threat_type;
      worst_match = match;
    }
  }

  *worst_sb_threat_type = SafetyNetJavaToSBThreatType(worst_threat_type);
  if (*worst_sb_threat_type == SB_THREAT_TYPE_SAFE || !worst_match)
    return UmaRemoteCallResult::JSON_UNKNOWN_THREAT;

  // Fill in the metadata
  metadata->population_id = ParseUserPopulation(*worst_match);
  if (*worst_sb_threat_type == SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    metadata->subresource_filter_match =
        ParseSubresourceFilterMatch(*worst_match);
  }

  return UmaRemoteCallResult::MATCH;  // success
}

}  // namespace safe_browsing
