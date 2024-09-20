// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing DB code.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_

#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/trace_event/traced_value.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

// Metadata that indicates what kind of URL match this is.
enum class ThreatPatternType : int {
  NONE = 0,                        // Pattern type didn't appear in the metadata
  MALWARE_LANDING = 1,             // The match is a malware landing page
  MALWARE_DISTRIBUTION = 2,        // The match is a malware distribution page
  SOCIAL_ENGINEERING_ADS = 3,      // The match is a social engineering ads page
  SOCIAL_ENGINEERING_LANDING = 4,  // The match is a social engineering landing
                                   // page
  PHISHING = 5,                    // The match is a phishing page
  THREAT_PATTERN_TYPE_MAX_VALUE
};

enum class SubresourceFilterType : int { ABUSIVE = 0, BETTER_ADS = 1 };

enum class SubresourceFilterLevel : int { WARN = 0, ENFORCE = 1 };

using SubresourceFilterMatch =
    base::flat_map<SubresourceFilterType, SubresourceFilterLevel>;

// Metadata that was returned by a GetFullHash call. This is the parsed version
// of the PB (from Pver3, or Pver4 local) or JSON (from Pver4 via GMSCore).
// Some fields are only applicable to certain lists.
//
// When adding elements to this struct, make sure you update operator== and
// ToTracedValue.
struct ThreatMetadata {
  ThreatMetadata();
  ThreatMetadata(const ThreatMetadata& other);
  ThreatMetadata(ThreatMetadata&& other);
  ThreatMetadata& operator=(const ThreatMetadata& other);
  ThreatMetadata& operator=(ThreatMetadata&& other);
  ~ThreatMetadata();

  bool operator==(const ThreatMetadata& other) const;
  bool operator!=(const ThreatMetadata& other) const;

  // Returns the metadata in a format tracing can support.
  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;

  // Type of blocklisted page. Used on malware and UwS lists.
  // This will be NONE if it wasn't present in the reponse.
  ThreatPatternType threat_pattern_type;

  // Set of permissions blocked. Used with threat_type API_ABUSE.
  // This will be empty if it wasn't present in the response.
  std::set<std::string> api_permissions;

  // Map of list sub-types related to the SUBRESOURCE_FILTER threat type.
  // Used instead of ThreatPatternType to allow multiple types at the same time.
  SubresourceFilterMatch subresource_filter_match;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_
