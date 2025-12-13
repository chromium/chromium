// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATIONS_GLOBAL_CACHE_LIST_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATIONS_GLOBAL_CACHE_LIST_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace safe_browsing {
// A list of domains which are known to send safe notification contents. For
// these domains, some notification protection will be omitted (e.g. suspicious
// notification warnings and behavior-based telemetry/enforcement).
std::vector<std::string>& GetNotificationsGlobalCacheListDomains();

// Test method to set domains for testing.
void SetNotificationsGlobalCacheListDomainsForTesting(
    std::vector<std::string> domains);

// Returns true if `url` is in the notifications global cache list or if the
// notifications global cache list is empty. The notification protections should
// only be run if the `url` is not on the global cache list, and if the list
// cannot be loaded then this cannot be verified.
bool ShouldSkipNotificationProtectionsDueToGlobalCacheList(const GURL& url);
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATIONS_GLOBAL_CACHE_LIST_H_
