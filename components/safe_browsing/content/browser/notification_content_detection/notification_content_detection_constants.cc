// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"

namespace safe_browsing {

const char kSuspiciousVerdictLabel[] = "suspicious";

const char kAllowlistCheckLatencyHistogram[] =
    "SafeBrowsing.NotificationContentDetection.AllowlistCheckLatency";
const char kSuspiciousScoreHistogram[] =
    "SafeBrowsing.NotificationContentDetection.SuspiciousScore";

const char kIsAllowlistedByUserKey[] = "is-allowlisted-by-user";

const char kSuspiciousNotificationIdsKey[] = "suspicious-notification-ids";

const char kMetadataDictionaryKey[] = "content-detection";
const char kMetadataSuspiciousKey[] = "suspicious-score";
const char kMetadataIsOriginOnGlobalCacheListKey[] =
    "is-origin-on-global-cache-list";
const char kMetadataIsOriginAllowlistedByUserKey[] =
    "is-origin-allowlisted-by-user";

}  // namespace safe_browsing
