// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/core/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/notification_wrapper_android.h"
#endif

RevokedPermissionsOSNotificationDisplayManager::
    RevokedPermissionsOSNotificationDisplayManager(
        scoped_refptr<HostContentSettingsMap> hcsm,
        std::unique_ptr<SafetyHubNotificationWrapper> notification_wrapper)
    : hcsm_(std::move(hcsm)),
      notification_wrapper_(std::move(notification_wrapper)) {}

RevokedPermissionsOSNotificationDisplayManager::
    ~RevokedPermissionsOSNotificationDisplayManager() = default;

void RevokedPermissionsOSNotificationDisplayManager::DisplayNotification() {
  if (notification_wrapper_) {
    notification_wrapper_->DisplayNotification(GetTotalRevocationCount());
  }
}

void RevokedPermissionsOSNotificationDisplayManager::UpdateNotification() {
  if (notification_wrapper_) {
    notification_wrapper_->UpdateNotification(GetTotalRevocationCount());
  }
}

int RevokedPermissionsOSNotificationDisplayManager::GetTotalRevocationCount() {
  std::set<GURL> revoked_urls;
  if (base::FeatureList::IsEnabled(
          safe_browsing::kAutoRevokeSuspiciousNotification)) {
    ContentSettingsForOneType revoked_abusive_notification_settings =
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm_.get());
    for (const auto& revoked_abusive_notification_permission :
         revoked_abusive_notification_settings) {
      const GURL abusive_url = GURL(
          revoked_abusive_notification_permission.primary_pattern.ToString());
      // Only suspicious notification should be count. Other Abusive
      // notification revocations are not included in the revocation
      // notification due to stronger confidence of sites being dangerous.
      if (AbusiveNotificationPermissionsManager::
              IsUrlRevokedDueToSuspiciousContent(hcsm_.get(), abusive_url)) {
        revoked_urls.insert(abusive_url);
      }
    }
  }
  ContentSettingsForOneType revoked_disruptive_notifications =
      DisruptiveNotificationPermissionsManager::GetRevokedNotifications(
          hcsm_.get());
  for (const auto& revoked_disruptive_notification_permission :
       revoked_disruptive_notifications) {
    const GURL disruptive_url = GURL(
        revoked_disruptive_notification_permission.primary_pattern.ToString());
    revoked_urls.insert(disruptive_url);
  }
  return revoked_urls.size();
}
