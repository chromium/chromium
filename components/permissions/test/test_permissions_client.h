// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_
#define COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_

#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permissions_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"

namespace permissions {

// PermissionsClient to be used in tests which implements all pure virtual
// methods. This class assumes only one BrowserContext will be used.
class TestPermissionsClient : public PermissionsClient {
 public:
  TestPermissionsClient();
  ~TestPermissionsClient() override;

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  privacy_sandbox::TrackingProtectionSettings* GetTrackingProtectionSettings(
      content::BrowserContext* browser_context) override;
  bool IsSubresourceFilterActivated(content::BrowserContext* browser_context,
                                    const GURL& url) override;
  OriginKeyedPermissionActionService* GetOriginKeyedPermissionActionService(
      content::BrowserContext* browser_context) override;
  PermissionActionsHistory* GetPermissionActionsHistory(
      content::BrowserContext* browser_context) override;
  PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  ObjectPermissionContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
  void GetUkmSourceId(ContentSettingsType permission_type,
                      content::BrowserContext* browser_context,
                      content::WebContents* web_contents,
                      const GURL& requesting_origin,
                      GetUkmSourceIdCallback callback) override;

  // Device (OS-level) simulated permissions
  bool HasDevicePermission(ContentSettingsType type) const override;
  bool CanRequestDevicePermission(ContentSettingsType type) const override;
  void SetHasDevicePermission(bool has_device_permission);
  void SetCanRequestDevicePermission(bool can_request_device_permission);

 private:
  TestPermissionsClient(const TestPermissionsClient&) = delete;
  TestPermissionsClient& operator=(const TestPermissionsClient&) = delete;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  PermissionDecisionAutoBlocker autoblocker_;
  PermissionActionsHistory permission_actions_history_;
  OriginKeyedPermissionActionService origin_keyed_permission_action_service_;
  bool has_device_permission_ = true;
  bool can_request_device_permission_ = false;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_
