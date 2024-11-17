// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/mock_safe_browsing_database_manager.h"

#include "base/containers/contains.h"

namespace safe_browsing {

MockSafeBrowsingDatabaseManager::MockSafeBrowsingDatabaseManager()
    : TestSafeBrowsingDatabaseManager(
          base::SequencedTaskRunner::GetCurrentDefault()) {}
MockSafeBrowsingDatabaseManager::~MockSafeBrowsingDatabaseManager() = default;

void MockSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& gurl,
    CheckUrlForHighConfidenceAllowlistCallback callback) {
  std::string url = gurl.spec();
  DCHECK(base::Contains(urls_allowlist_match_, url));
  if (base::Contains(delayed_url_callbacks_, url)) {
    delayed_url_callbacks_[url] = std::move(callback);
  } else {
    std::move(callback).Run(urls_allowlist_match_[url],
                            /*logging_details=*/std::nullopt);
  }
}

void MockSafeBrowsingDatabaseManager::SetAllowlistLookupDetailsForUrl(
    const GURL& gurl,
    bool match) {
  std::string url = gurl.spec();
  urls_allowlist_match_[url] = match;
}

void MockSafeBrowsingDatabaseManager::SetCallbackToDelayed(const GURL& gurl) {
  std::string url = gurl.spec();
  delayed_url_callbacks_[url] = base::NullCallback();
}

void MockSafeBrowsingDatabaseManager::RestartDelayedCallback(const GURL& gurl) {
  std::string url = gurl.spec();
  DCHECK(base::Contains(urls_allowlist_match_, url));
  DCHECK(base::Contains(delayed_url_callbacks_, url));
  DCHECK(delayed_url_callbacks_[url]);
  std::move(delayed_url_callbacks_[url])
      .Run(urls_allowlist_match_[url], /*logging_details=*/std::nullopt);
}

}  // namespace safe_browsing
