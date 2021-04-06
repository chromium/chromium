// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/test_subresource_filter_client.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

namespace subresource_filter {

TestSubresourceFilterClient::TestSubresourceFilterClient(
    content::WebContents* web_contents) {
  // Set up the state that's required by ProfileInteractionManager.
  HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
  settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
      &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
      false /* restore_session */);
  profile_context_ =
      std::make_unique<SubresourceFilterProfileContext>(settings_map_.get());

  // ProfileInteractionManager assumes that this object is present in the
  // context of the passed-in WebContents.
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr, settings_map_.get()));
}

TestSubresourceFilterClient::~TestSubresourceFilterClient() {
  settings_map_->ShutdownOnUIThread();
}

void TestSubresourceFilterClient::ShowNotification() {
  ++disallowed_notification_count_;
}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
TestSubresourceFilterClient::GetSafeBrowsingDatabaseManager() {
  return database_manager_;
}

void TestSubresourceFilterClient::CreateSafeBrowsingDatabaseManager() {
  database_manager_ = base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();
}

void TestSubresourceFilterClient::SetShouldUseSmartUI(bool enabled) {
  profile_context_->settings_manager()->set_should_use_smart_ui_for_testing(
      enabled);
}

}  // namespace subresource_filter
