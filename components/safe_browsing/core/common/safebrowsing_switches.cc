// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing::switches {

//
// Safe Browsing Lookup/Update API switches
//

// List of comma-separated URLs to mark as phishing for local database checks.
const char kMarkAsPhishing[] = "mark_as_phishing";
// List of comma-separated URLs to mark as malware for local database checks.
const char kMarkAsMalware[] = "mark_as_malware";
// List of comma-separated URLs to mark as unwanted software for local database
// checks.
const char kMarkAsUws[] = "mark_as_uws";
// List of comma-separated URLs to mark as present on the high-confidence
// allowlist.
const char kMarkAsHighConfidenceAllowlisted[] =
    "mark_as_allowlisted_for_real_time";
// Command-line flag for caching an artificial phishing verdict for URL
// real-time lookups.
const char kArtificialCachedUrlRealTimeVerdictFlag[] =
    "mark_as_real_time_phishing";
// Command-line flag for caching an artificial phishing verdict for hash-prefix
// real-time lookups.
const char kArtificialCachedHashPrefixRealTimeVerdictFlag[] =
    "mark_as_hash_prefix_real_time_phishing";
// Command-line flag for caching an artificial blocked enterprise lookup
// verdict.
const char kArtificialCachedEnterpriseBlockedVerdictFlag[] =
    "mark_as_enterprise_blocked";
// Command-line flag for caching an artificial flagged enterprise lookup
// verdict.
const char kArtificialCachedEnterpriseWarnedVerdictFlag[] =
    "mark_as_enterprise_warned";
// If the switch is present, any high-confidence allowlist check will return
// that it does not match the allowlist.
const char kSkipHighConfidenceAllowlist[] =
    "safe-browsing-skip-high-confidence-allowlist";
// Alternate URL to use for ChromeEnterpriseRealTimeUrlLookupService real-time
// lookups.
const char kUrlFilteringEndpointFlag[] = "url-filtering-endpoint";

//
// Client side detection switches
//

// Command-line flag that can be used to write extracted CSD features to disk.
// This is also enables a few other behaviors that are useful for debugging.
const char kCsdDebugFeatureDirectoryFlag[] = "csd-debug-feature-directory";
// If present, indicates that the client-side detection allowlist check should
// be skipped (it is treated as no match).
const char kSkipCSDAllowlistOnPreclassification[] =
    "safe-browsing-skip-csd-allowlist";
// Command-line flag that can be used to override the current CSD model. Must be
// provided with an absolute path.
const char kOverrideCsdModelFlag[] = "csd-model-override-path";

//
// Password protection switches
//

// Command-line flag for caching an artificial PhishGuard unsafe verdict.
const char kArtificialCachedPhishGuardVerdictFlag[] =
    "mark_as_phish_guard_phishing";
// List of comma-separated URLs to mark as present on the password protection
// allowlist. Note this uses the client-side detection allowlist.
const char kMarkAsPasswordProtectionAllowlisted[] =
    "mark_as_allowlisted_for_phish_guard";

//
// Cloud content scanning switches
//

// The command line flag to control the max amount of concurrent active
// requests.
const char kWpMaxParallelActiveRequests[] = "wp-max-parallel-active-requests";
const char kWpMaxFileOpeningThreads[] = "wp-max-file-opening-threads";
const char kCloudBinaryUploadServiceUrlFlag[] = "binary-upload-service-url";

//
// Miscellaneous switches
//

// List of comma-separated sha256 hashes of executable files which the
// download-protection service should treat as "dangerous."  For a file to
// show a warning, it also must be considered a dangerous filetype and not
// be allowlisted otherwise (by signature or URL) and must be on a supported
// OS. Hashes are in hex. This is used for manual testing when looking
// for ways to by-pass download protection.
const char kSbManualDownloadBlocklist[] =
    "safebrowsing-manual-download-blacklist";
// Enable Safe Browsing Enhanced Protection.
const char kSbEnableEnhancedProtection[] =
    "safebrowsing-enable-enhanced-protection";
const char kForceTreatUserAsAdvancedProtection[] =
    "safe-browsing-treat-user-as-advanced-protection";

// Enable the keyboard lock trigger of Scam Detection via command line for
// easier testing.
const char kScamDetectionKeyboardLockTriggerAndroid[] =
    "scam-detection-keyboard-lock-trigger-android";

}  // namespace safe_browsing::switches
