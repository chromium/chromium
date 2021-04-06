// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_SUBRESOURCE_FILTER_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

class HostContentSettingsMap;

namespace content {
class WebContents;
}

namespace subresource_filter {

class SubresourceFilterProfileContext;

// An implementation of SubresourceFilterClient suitable for use in unittests.
class TestSubresourceFilterClient : public SubresourceFilterClient {
 public:
  explicit TestSubresourceFilterClient(content::WebContents* web_contents);
  ~TestSubresourceFilterClient() override;

  // SubresourceFilterClient:
  void ShowNotification() override;
  const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() override;

  // GetSafeBrowsingDatabaseManager() returns null by default. Invoke this
  // method to change that behavior.
  void CreateSafeBrowsingDatabaseManager();

  SubresourceFilterProfileContext* profile_context() {
    return profile_context_.get();
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database_manager() {
    return database_manager_.get();
  }

  // Turns on/off the smart UI feature (currently enabled in production on
  // some platforms only).
  void SetShouldUseSmartUI(bool enabled);

  // Returns the number of times that ShowNotification() was invoked.
  int disallowed_notification_count() const {
    return disallowed_notification_count_;
  }

 private:
  scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  std::unique_ptr<SubresourceFilterProfileContext> profile_context_;

  int disallowed_notification_count_ = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_TEST_SUBRESOURCE_FILTER_CLIENT_H_
