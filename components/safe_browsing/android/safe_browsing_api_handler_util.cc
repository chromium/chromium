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

// Do not reorder or delete.  Make sure changes are reflected in
// SB2RemoteCallThreatSubType.
enum UmaThreatSubType {
  UMA_THREAT_SUB_TYPE_NOT_SET = 0,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_LANDING = 1,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_DISTRIBUTION = 2,
  UMA_THREAT_SUB_TYPE_UNKNOWN = 3,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_ADS = 4,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_LANDING = 5,
  UMA_THREAT_SUB_TYPE_PHISHING = 6,

  // DEPRECATED.
  UMA_THREAT_SUB_TYPE_BETTER_ADS = 7,
  UMA_THREAT_SUB_TYPE_ABUSIVE = 8,
  UMA_THREAT_SUB_TYPE_ALL_ADS = 9,

  UMA_THREAT_SUB_TYPE_MAX_VALUE
};

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
int GetThreatSeverity(JavaThreatTypes threat_type) {
  switch (threat_type) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return 0;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return 1;
    case JAVA_THREAT_TYPE_UNWANTED_SOFTWARE:
      return 2;
    case JAVA_THREAT_TYPE_SUBRESOURCE_FILTER:
      return 3;
    case JAVA_THREAT_TYPE_BILLING:
      return 4;
    case JAVA_THREAT_TYPE_CSD_ALLOWLIST:
      return 5;
    case JAVA_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      return 6;
    case JAVA_THREAT_TYPE_MAX_VALUE:
      return std::numeric_limits<int>::max();
  }
  NOTREACHED() << "Unhandled threat_type: " << threat_type;
  return std::numeric_limits<int>::max();
}

SBThreatType JavaToSBThreatType(int java_threat_num) {
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case JAVA_THREAT_TYPE_UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case JAVA_THREAT_TYPE_SUBRESOURCE_FILTER:
      return SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    case JAVA_THREAT_TYPE_BILLING:
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
    return UMA_STATUS_JSON_EMPTY;

  // Pick out the "matches" list.
  absl::optional<base::Value> value = base::JSONReader::Read(metadata_str);
  const base::Value::List* matches = nullptr;
  {
    if (!value.has_value())
      return UMA_STATUS_JSON_FAILED_TO_PARSE;

    base::Value::Dict* dict = value->GetIfDict();
    if (!dict)
      return UMA_STATUS_JSON_FAILED_TO_PARSE;

    matches = dict->FindList(kJsonKeyMatches);
    if (!matches)
      return UMA_STATUS_JSON_FAILED_TO_PARSE;
  }

  // Go through each matched threat type and pick the most severe.
  JavaThreatTypes worst_threat_type = JAVA_THREAT_TYPE_MAX_VALUE;
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

    JavaThreatTypes threat_type = static_cast<JavaThreatTypes>(threat_type_num);
    if (threat_type > JAVA_THREAT_TYPE_MAX_VALUE) {
      threat_type = JAVA_THREAT_TYPE_MAX_VALUE;
    }
    if (GetThreatSeverity(threat_type) < GetThreatSeverity(worst_threat_type)) {
      worst_threat_type = threat_type;
      worst_match = match;
    }
  }

  *worst_sb_threat_type = JavaToSBThreatType(worst_threat_type);
  if (*worst_sb_threat_type == SB_THREAT_TYPE_SAFE || !worst_match)
    return UMA_STATUS_JSON_UNKNOWN_THREAT;

  // Fill in the metadata
  metadata->population_id = ParseUserPopulation(*worst_match);
  if (*worst_sb_threat_type == SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    metadata->subresource_filter_match =
        ParseSubresourceFilterMatch(*worst_match);
  }

  return UMA_STATUS_MATCH;  // success
}

}  // namespace safe_browsing
