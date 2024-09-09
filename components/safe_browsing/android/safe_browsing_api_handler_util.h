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

// Threat types as per the Java code.
// This must match those in SafeBrowsingThreat.java in GMS's SafetyNet API.
enum class SafetyNetJavaThreatType {
  // Magic numbers for allowlists. Not actually used by GMSCore.
  CSD_ALLOWLIST = 16,
  MAX_VALUE
};

// Must match what SafeBrowsingApiHandler.java uses for |lookupResult|.
// This is self-defined enum in Chromium. The difference between this enum and
// the |SafeBrowsingJavaResponseStatus| enum is that this enum represents the
// call result to the API (e.g. not able to connect, timed out, invalid input)
// while |SafeBrowsingJavaResponseStatus| is obtained directly from the API
// response in a successful call. In other words, ResponseStatus is valid only
// when LookupResult is SUCCESS.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafeBrowsingApiLookupResult {
  SUCCESS = 0,
  // General failure bucket. This is set if none of the more granular failure
  // buckets fits.
  FAILURE = 1,
  // The API call to the Safe Browsing API timed out.
  FAILURE_API_CALL_TIMEOUT = 2,
  // The API throws an UnsupportedApiCallException.
  FAILURE_API_UNSUPPORTED = 3,
  // The API throws an ApiException with API_UNAVAILABLE status code.
  FAILURE_API_NOT_AVAILABLE = 4,
  // The API handler is null. Should never happen in production.
  FAILURE_HANDLER_NULL = 5
};

// Must match the definition in SafeBrowsing::ThreatType in SafeBrowsing API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Please update the hard coded value in MockSafeBrowsingApiHandler if
// values are changed.
enum class SafeBrowsingJavaThreatType {
  NO_THREAT = 0,
  SOCIAL_ENGINEERING = 2,
  UNWANTED_SOFTWARE = 3,
  POTENTIALLY_HARMFUL_APPLICATION = 4,
  BILLING = 15,
  ABUSIVE_EXPERIENCE_VIOLATION = 20,
  BETTER_ADS_VIOLATION = 21
};

// Must match the definition in SafeBrowsing::ThreatAttribute in SafeBrowsing
// API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafeBrowsingJavaThreatAttribute { CANARY = 1, FRAME_ONLY = 2 };

// Must match the definition in SafeBrowsing::Protocol in the SafeBrowsing
// API.
enum class SafeBrowsingJavaProtocol { LOCAL_BLOCK_LIST = 4, REAL_TIME = 5 };

// Must match the definition in SafeBrowsingResponse::SafeBrowsingResponseStatus
// in SafeBrowsing API. This enum is converted directly from the API response.
// See the comment above |SafeBrowsingApiLookupResult| for the difference
// between the two enums.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafeBrowsingJavaResponseStatus {
  SUCCESS_WITH_LOCAL_BLOCKLIST = 0,
  SUCCESS_WITH_REAL_TIME = 1,
  SUCCESS_FALLBACK_REAL_TIME_TIMEOUT = 2,
  SUCCESS_FALLBACK_REAL_TIME_THROTTLED = 3,
  FAILURE_NETWORK_UNAVAILABLE = 4,
  FAILURE_BLOCK_LIST_UNAVAILABLE = 5,
  FAILURE_INVALID_URL = 6
};

// The result logged when validating the response from SafeBrowsing API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafeBrowsingJavaValidationResult {
  VALID = 0,
  VALID_WITH_UNRECOGNIZED_RESPONSE_STATUS = 1,
  INVALID_LOOKUP_RESULT = 2,
  INVALID_THREAT_TYPE = 3,
  INVALID_THREAT_ATTRIBUTE = 4,

  kMaxValue = INVALID_THREAT_ATTRIBUTE
};

// LINT.IfChange(VerifyAppsEnabledResult)
// The result of either SafetyNet.isVerifyAppsEnabled or
// SafetyNet.enableVerifyApps. These values are persisted to
// logs. Entries should not be renumbered and numeric values should
// never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.safe_browsing
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: VerifyAppsResult
enum class VerifyAppsEnabledResult {
  SUCCESS_ENABLED = 0,
  SUCCESS_NOT_ENABLED = 1,
  TIMEOUT = 2,
  FAILED = 3,
  kMaxValue = FAILED,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SafeBrowsingVerifyAppsEnabledResult)

// Translates |threat_type| and |threat_attributes| from the Safe Browsing API
// into ThreatMetadata.
ThreatMetadata GetThreatMetadataFromSafeBrowsingApi(
    SafeBrowsingJavaThreatType threat_type,
    const std::vector<int>& threat_attributes);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_UTIL_H_
