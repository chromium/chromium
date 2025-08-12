// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"

#include "base/no_destructor.h"
#include "base/strings/string_tokenizer.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace safe_browsing {

std::vector<std::string>& GetNotificationsGlobalCacheListDomains() {
  static base::NoDestructor<std::vector<std::string>> list([] {
    std::vector<std::string> temp_list;
    std::string newline_separted_origins =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_NOTIFICATIONS_GLOBAL_CACHE_ORIGINS);
    base::StringTokenizer t(newline_separted_origins, "\n");
    while (std::optional<std::string_view> token = t.GetNextTokenView()) {
      // Convert std::string_view to std::string for storage.
      temp_list.push_back(std::string(*token));
    }
    return temp_list;
  }());
  return *list;
}

void SetNotificationsGlobalCacheListDomainsForTesting(
    std::vector<std::string> domains) {
  GetNotificationsGlobalCacheListDomains().swap(domains);
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
