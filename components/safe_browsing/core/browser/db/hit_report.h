// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Datastructures that hold details of a Safe Browsing hit for reporting.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HIT_REPORT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HIT_REPORT_H_

#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "url/gurl.h"

namespace safe_browsing {

// What service classified this threat as unsafe.
enum class ThreatSource {
  UNKNOWN,
  LOCAL_PVER4,            // From V4LocalDatabaseManager, protocol v4
  REMOTE,                 // From RemoteSafeBrowsingDatabaseManager
  CLIENT_SIDE_DETECTION,  // From ClientSideDetectionHost
  REAL_TIME_CHECK,        // From RealTimeUrlLookupService
};

// Data to report about the contents of a particular threat (malware, phishing,
// unsafe download URL).  If post_data is non-empty, the request will be
// sent as a POST instead of a GET.
struct HitReport {
  HitReport();
  HitReport(const HitReport& other);
  ~HitReport();

  GURL malicious_url;
  GURL page_url;
  GURL referrer_url;

  bool is_subresource;
  SBThreatType threat_type;
  ThreatSource threat_source;

  // Opaque string used for tracking Pver4-based experiments.
  // NOTE(vakh): Unused at the moment, but may be used later.
  std::string population_id;

  ExtendedReportingLevel extended_reporting_level;
  bool is_enhanced_protection = false;
  bool is_metrics_reporting_active;

  std::string post_data;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HIT_REPORT_H_
