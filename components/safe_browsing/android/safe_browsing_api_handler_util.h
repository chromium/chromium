// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions for SafeBrowsingApiHandlerImpl.  Separated out for tests.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_

#include <string>

#include "components/safe_browsing/core/browser/db/util.h"

namespace safe_browsing {

// These match what SafetyNetApiHandler.java uses for |resultStatus|
enum class SafetyNetRemoteCallResultStatus {
  INTERNAL_ERROR = -1,
  SUCCESS = 0,
  TIMEOUT = 1,
};

// Threat types as per the Java code.
// This must match those in SafeBrowsingThreat.java in GMS's SafetyNet API.
enum class SafetyNetJavaThreatType {
  UNWANTED_SOFTWARE = 3,
  POTENTIALLY_HARMFUL_APPLICATION = 4,
  SOCIAL_ENGINEERING = 5,
  SUBRESOURCE_FILTER = 13,
  BILLING = 15,
  // Magic numbers for allowlists. Not actually used by GMSCore.
  CSD_ALLOWLIST = 16,
  MAX_VALUE
};

// Do not reorder or delete entries, and make sure changes here are reflected
// in SB2RemoteCallResult histogram.
enum class UmaRemoteCallResult {
  INTERNAL_ERROR = 0,
  TIMEOUT = 1,
  SAFE = 2,
  MATCH = 3,
  JSON_EMPTY = 4,
  JSON_FAILED_TO_PARSE = 5,
  JSON_UNKNOWN_THREAT = 6,
  UNSUPPORTED = 7,
  MAX_VALUE
};

// This parses the JSON from the GMSCore API and then:
//   1) Picks the most severe threat type
//   2) Parses that threat's key/value pairs into the metadata struct.
//
// If anything fails to parse, this sets the threat to "safe".  The caller
// should report the return value via UMA.
UmaRemoteCallResult ParseJsonFromGMSCore(const std::string& metadata_str,
                                         SBThreatType* worst_threat,
                                         ThreatMetadata* metadata);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_
