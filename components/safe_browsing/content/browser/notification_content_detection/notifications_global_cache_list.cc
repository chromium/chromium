// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"

#include "base/no_destructor.h"

namespace {
std::vector<std::string>& GetNotificationsGlobalCacheListDomainsInternal() {
  static base::NoDestructor<std::vector<std::string>> g_domains;
  return *g_domains;
}
}  // namespace

namespace safe_browsing {

const std::vector<std::string>& GetNotificationsGlobalCacheListDomains() {
  return GetNotificationsGlobalCacheListDomainsInternal();
}

void SetNotificationsGlobalCacheListDomainsForTesting(
    std::vector<std::string> domains) {
  GetNotificationsGlobalCacheListDomainsInternal().swap(domains);
}

bool IsDomainInNotificationsGlobalCacheList(const GURL& url) {
  const std::vector<std::string>& domains =
      GetNotificationsGlobalCacheListDomains();
  for (const std::string& domain : domains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

}  // namespace safe_browsing
