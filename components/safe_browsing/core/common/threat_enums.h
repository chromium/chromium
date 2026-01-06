// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Enums for specifying details of Safe Browsing threat reporting.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAT_ENUMS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAT_ENUMS_H_

namespace safe_browsing {

// What service classified this threat as unsafe.
enum class ThreatSource {
  UNKNOWN,
  // From V4LocalDatabaseManager, protocol v4. Desktop only.
  LOCAL_PVER4,
  // From ClientSideDetectionHost.
  CLIENT_SIDE_DETECTION,
  // From RealTimeUrlLookupService. Not including fallback to protocol v4.
  URL_REAL_TIME_CHECK,
  // From HashRealTimeService. Desktop only. Not including fallback to
  // protocol v4.
  NATIVE_PVER5_REAL_TIME,
  // From GmsCore SafeBrowsing API. Android only. Including fallback to protocol
  // v4 (through either SafeBrowsing API or SafetyNet API).
  ANDROID_SAFEBROWSING_REAL_TIME,
  // From GmsCore SafeBrowsing API. Android only. Protocol v4 only.
  ANDROID_SAFEBROWSING,
};

// What subtype that expands more into details on what threat category
// SBThreatType is targeting.
enum class ThreatSubtype {
  UNKNOWN,
  // Scam experiment verdict 1
  SCAM_EXPERIMENT_VERDICT_1,
  // Scam experiment verdict 2
  SCAM_EXPERIMENT_VERDICT_2,
  // Scam experiment catch all enforcement
  SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT,
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAT_ENUMS_H_
