// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions for SafeBrowsingApiHandlerImpl.  Separated out for tests.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_

#include <string>

#include "components/safe_browsing/db/util.h"

namespace safe_browsing {

// These match what SafeBrowsingApiHandler.java uses for |resultStatus|
enum RemoteCallResultStatus {
  RESULT_STATUS_INTERNAL_ERROR = -1,
  RESULT_STATUS_SUCCESS = 0,
  RESULT_STATUS_TIMEOUT = 1,
};

// Threat types as per the Java code.
// This must match those in GMS's SafeBrowsingThreat.java.
enum JavaThreatTypes {
  JAVA_THREAT_TYPE_UNWANTED_SOFTWARE = 3,
  JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION = 4,
  JAVA_THREAT_TYPE_SOCIAL_ENGINEERING = 5,
  JAVA_THREAT_TYPE_SUBRESOURCE_FILTER = 13,
  JAVA_THREAT_TYPE_BILLING = 15,
  // TODO(crbug.com/999344): temp magic number, update once GMSCore is
  // available.
  JAVA_THREAT_TYPE_CSD_ALLOWLIST = 16,
  JAVA_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST = 17,
  JAVA_THREAT_TYPE_MAX_VALUE
};

// Do not reorder or delete entries, and make sure changes here are reflected
// in SB2RemoteCallResult histogram.
enum UmaRemoteCallResult {
  UMA_STATUS_INTERNAL_ERROR = 0,
  UMA_STATUS_TIMEOUT = 1,
  UMA_STATUS_SAFE = 2,
  UMA_STATUS_MATCH = 3,
  UMA_STATUS_JSON_EMPTY = 4,
  UMA_STATUS_JSON_FAILED_TO_PARSE = 5,
  UMA_STATUS_JSON_UNKNOWN_THREAT = 6,
  UMA_STATUS_UNSUPPORTED = 7,
  UMA_STATUS_MAX_VALUE
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

// DEPRECATED.  Will be removed.
UmaRemoteCallResult ParseJsonToThreatAndPB(const std::string& metadata_str,
                                           SBThreatType* worst_threat,
                                           std::string* metadata_pb_str);
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_
