// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/test_permissions_client.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/permissions/permission_actions_history.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"

namespace permissions {
namespace {

scoped_refptr<HostContentSettingsMap> CreateSettingsMap(
    sync_preferences::TestingPrefServiceSyncable* prefs) {
  HostContentSettingsMap::RegisterProfilePrefs(prefs->registry());
  return base::MakeRefCounted<HostContentSettingsMap>(
      prefs, false /* is_off_the_record */, false /* store_last_modified */,
      false /* restore_session */, false /* should_record_metrics */);
}

}  // namespace

TestPermissionsClient::TestPermissionsClient()
    : settings_map_(CreateSettingsMap(&prefs_)),
      autoblocker_(settings_map_.get()),
      permission_actions_history_(&prefs_) {
  PermissionActionsHistory::RegisterProfilePrefs(prefs_.registry());
}

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

privacy_sandbox::TrackingProtectionSettings*
TestPermissionsClient::GetTrackingProtectionSettings(
    content::BrowserContext* browser_context) {
  return nullptr;
}

bool TestPermissionsClient::IsSubresourceFilterActivated(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

OriginKeyedPermissionActionService*
TestPermissionsClient::GetOriginKeyedPermissionActionService(
    content::BrowserContext* browser_context) {
  return &origin_keyed_permission_action_service_;
}

PermissionActionsHistory* TestPermissionsClient::GetPermissionActionsHistory(
    content::BrowserContext* browser_context) {
  return &permission_actions_history_;
}

PermissionDecisionAutoBlocker*
TestPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return &autoblocker_;
}

ObjectPermissionContextBase* TestPermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return nullptr;
}

void TestPermissionsClient::GetUkmSourceId(
    ContentSettingsType permission_type,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    GetUkmSourceIdCallback callback) {
  if (web_contents) {
    ukm::SourceId source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    std::move(callback).Run(source_id);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

bool TestPermissionsClient::HasDevicePermission(
    ContentSettingsType type) const {
  return has_device_permission_;
}

bool TestPermissionsClient::CanRequestDevicePermission(
    ContentSettingsType type) const {
  return can_request_device_permission_;
}

void TestPermissionsClient::SetHasDevicePermission(bool has_device_permission) {
  has_device_permission_ = has_device_permission;
}

void TestPermissionsClient::SetCanRequestDevicePermission(
    bool can_request_device_permission) {
  can_request_device_permission_ = can_request_device_permission;
}

}  // namespace permissions
