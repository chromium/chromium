// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/test_permissions_client.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/ukm/content/source_url_recorder.h"

namespace permissions {
namespace {

scoped_refptr<HostContentSettingsMap> CreateSettingsMap(
    sync_preferences::TestingPrefServiceSyncable* prefs) {
  HostContentSettingsMap::RegisterProfilePrefs(prefs->registry());
  return base::MakeRefCounted<HostContentSettingsMap>(
      prefs, false /* is_off_the_record */, false /* store_last_modified */,
      false /* restore_session */);
}

}  // namespace

TestPermissionsClient::TestPermissionsClient()
    : settings_map_(CreateSettingsMap(&prefs_)),
      autoblocker_(settings_map_.get()) {}

TestPermissionsClient::~TestPermissionsClient() {
  settings_map_->ShutdownOnUIThread();
}

HostContentSettingsMap* TestPermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return settings_map_.get();
}

scoped_refptr<content_settings::CookieSettings>
TestPermissionsClient::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return nullptr;
}

bool TestPermissionsClient::IsSubresourceFilterActivated(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

PermissionDecisionAutoBlocker*
TestPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return &autoblocker_;
}

PermissionManager* TestPermissionsClient::GetPermissionManager(
    content::BrowserContext* browser_context) {
  return nullptr;
}

ChooserContextBase* TestPermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return nullptr;
}

void TestPermissionsClient::GetUkmSourceId(
    content::BrowserContext* browser_context,
    const content::WebContents* web_contents,
    const GURL& requesting_origin,
    GetUkmSourceIdCallback callback) {
  if (web_contents) {
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebContentsDocument(web_contents);
    std::move(callback).Run(source_id);
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

}  // namespace permissions
