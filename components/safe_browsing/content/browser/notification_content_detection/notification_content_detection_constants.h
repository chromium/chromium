// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_CONSTANTS_H_

namespace safe_browsing {

// Model's label for a suspicious verdict.
extern const char kSuspiciousVerdictLabel[];

// Histogram names.
extern const char kAllowlistCheckLatencyHistogram[];
extern const char kSuspiciousScoreHistogram[];

// Website setting value key for user's allowlist.
extern const char kIsAllowlistedByUserKey[];

// Website setting value key for suspicious notification ids.
extern const char kSuspiciousNotificationIdsKey[];

// MQLS metadata dictionary.
extern const char kMetadataDictionaryKey[];
extern const char kMetadataSuspiciousKey[];
extern const char kMetadataIsOriginOnGlobalCacheListKey[];
extern const char kMetadataIsOriginAllowlistedByUserKey[];

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_CONSTANTS_H_
