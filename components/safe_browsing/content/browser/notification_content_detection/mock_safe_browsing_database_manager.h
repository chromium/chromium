// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_

#include "components/safe_browsing/core/browser/db/test_database_manager.h"

namespace safe_browsing {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager();

  // Calls the callback with the allowlist match result previously set by
  // |SetAllowlistLookupDetailsForUrl|. It crashes if the allowlist match result
  // is not set in advance for the |gurl|.
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      CheckUrlForHighConfidenceAllowlistCallback callback) override;

  void SetAllowlistLookupDetailsForUrl(const GURL& gurl, bool match);

  void SetCallbackToDelayed(const GURL& gurl);

  void RestartDelayedCallback(const GURL& gurl);

 protected:
  friend class NotificationContentDetectionServiceTest;
  ~MockSafeBrowsingDatabaseManager() override;

 private:
  base::flat_map<std::string, bool> urls_allowlist_match_;
  base::flat_map<std::string, CheckUrlForHighConfidenceAllowlistCallback>
      delayed_url_callbacks_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_
