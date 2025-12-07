// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_SWITCHES_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_SWITCHES_H_

namespace safe_browsing::switches {

extern const char kMarkAsPhishing[];
extern const char kMarkAsMalware[];
extern const char kMarkAsUws[];
extern const char kMarkAsHighConfidenceAllowlisted[];
extern const char kArtificialCachedUrlRealTimeVerdictFlag[];
extern const char kArtificialCachedHashPrefixRealTimeVerdictFlag[];
extern const char kArtificialCachedEnterpriseBlockedVerdictFlag[];
extern const char kArtificialCachedEnterpriseWarnedVerdictFlag[];
extern const char kSkipHighConfidenceAllowlist[];
extern const char kUrlFilteringEndpointFlag[];
extern const char kCsdDebugFeatureDirectoryFlag[];
extern const char kSkipCSDAllowlistOnPreclassification[];
extern const char kOverrideCsdModelFlag[];
extern const char kArtificialCachedPhishGuardVerdictFlag[];
extern const char kMarkAsPasswordProtectionAllowlisted[];
extern const char kWpMaxParallelActiveRequests[];
extern const char kWpMaxFileOpeningThreads[];
extern const char kCloudBinaryUploadServiceUrlFlag[];
extern const char kSbManualDownloadBlocklist[];
extern const char kSbEnableEnhancedProtection[];
extern const char kForceTreatUserAsAdvancedProtection[];
extern const char kScamDetectionKeyboardLockTriggerAndroid[];

}  // namespace safe_browsing::switches

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_SWITCHES_H_
