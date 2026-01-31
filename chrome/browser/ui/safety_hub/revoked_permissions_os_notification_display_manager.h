// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_H_

#include <set>

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"

// Manages OS notifications (i.e. Android) shown to the user when notification
// permissions are revoked, either for being disruptive or abusive.
class RevokedPermissionsOSNotificationDisplayManager : public KeyedService {
 public:
  // Wrapper for platform-specific notifications.
  class SafetyHubNotificationWrapper {
   public:
    virtual ~SafetyHubNotificationWrapper() = default;
    virtual void DisplayNotification(int count,
                                     std::string& first_affected_domain,
                                     bool any_suspicious_revocations,
                                     bool any_disruptive_revocations) = 0;
    virtual void UpdateNotification(int count,
                                    std::string& first_affected_domain,
                                    bool any_suspicious_revocations,
                                    bool any_disruptive_revocations) = 0;
  };

  explicit RevokedPermissionsOSNotificationDisplayManager(
      scoped_refptr<HostContentSettingsMap> hcsm,
      std::unique_ptr<SafetyHubNotificationWrapper> notification_wrapper);
  ~RevokedPermissionsOSNotificationDisplayManager() override;

  // Triggers a notification to be displayed with the total count of revoked
  // permissions.
  virtual void DisplayNotification();

  // Triggers a notification to be updated with the total count of revoked
  // permissions.
  virtual void UpdateNotification();

 private:
  std::set<GURL> GetRevocationUrls();
  bool IsAnyRevocationDueToSuspiciousContent(
      const std::set<GURL>& revoked_urls);
  bool IsAnyRevocationDueToDisruptiveContent(
      const std::set<GURL>& revoked_urls);

  scoped_refptr<HostContentSettingsMap> hcsm_;
  std::unique_ptr<SafetyHubNotificationWrapper> notification_wrapper_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_H_
