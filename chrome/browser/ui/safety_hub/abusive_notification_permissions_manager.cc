// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

AbusiveNotificationPermissionsManager::AbusiveNotificationPermissionsManager(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<HostContentSettingsMap> hcsm)
    : database_manager_(database_manager),
      hcsm_(hcsm),
      safe_browsing_check_delay_(kCheckUrlTimeoutMs) {}

AbusiveNotificationPermissionsManager::
    ~AbusiveNotificationPermissionsManager() = default;

ContentSettingsForOneType
AbusiveNotificationPermissionsManager::GetRevokedPermissions() const {
  return hcsm_->GetSettingsForOneType(
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS);
}

void AbusiveNotificationPermissionsManager::
    CheckNotificationPermissionOrigins() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!database_manager_) {
    return;
  }
  ResetSafeBrowsingCheckHelpers();
  auto notification_permission_settings =
      hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS);
  for (const auto& setting : notification_permission_settings) {
    // Ignore origins where the permission is not CONTENT_SETTING_ALLOW or the
    // user previously bypassed a warning or re-allowed the permission in Safety
    // Hub.
    if (ShouldCheckOrigin(setting)) {
      // Since we are dealing with permissions for specific origins, and there
      // are no wildcard values in the pattern, it is safe to convert between
      // ContentSettingsPattern, string, and URL types.
      GURL setting_url = GURL(setting.primary_pattern.ToString());
      PerformSafeBrowsingChecks(setting_url);
    }
  }
}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    SafeBrowsingCheckClient(
        safe_browsing::SafeBrowsingDatabaseManager* database_manager,
        std::set<std::string>* abusive_origins,
        raw_ptr<std::map<SafeBrowsingCheckClient*,
                         std::unique_ptr<SafeBrowsingCheckClient>>>
            safe_browsing_request_clients,
        GURL url,
        int safe_browsing_check_delay)
    : database_manager_(database_manager),
      abusive_origins_(abusive_origins),
      safe_browsing_request_clients_(safe_browsing_request_clients),
      url_(url),
      safe_browsing_check_delay_(safe_browsing_check_delay) {}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    ~SafeBrowsingCheckClient() {
  if (timer_.IsRunning()) {
    database_manager_->CancelCheck(this);
    timer_.Stop();
  }
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    CheckSocialEngineeringBlocklist() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Set a timer to fail open, i.e. call it "allowlisted", if the full
  // check takes too long.
  auto timeout_callback =
      base::BindOnce(&AbusiveNotificationPermissionsManager::
                         SafeBrowsingCheckClient::OnCheckBlocklistTimeout,
                     weak_factory_.GetWeakPtr());

  // Start a timer to abort the check if it takes too long.
  timer_.Start(FROM_HERE, base::Milliseconds(safe_browsing_check_delay_),
               std::move(timeout_callback));

  // Check the phishing blocklist, since this is where we'll find blocklisted
  // abusive notification sites.
  bool is_safe_synchronously = database_manager_->CheckBrowseUrl(
      url_,
      safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING}),
      this, safe_browsing::CheckBrowseUrlType::kHashDatabase);
  // If we can synchronously determine that the URL is safe, stop the timer to
  // avoid `OnCheckBlocklistTimeout` from being called, since
  // `OnCheckBrowseUrlResult` won't be called.
  if (is_safe_synchronously) {
    timer_.Stop();
    safe_browsing_request_clients_->erase(this);
    // The previous line results in deleting this object.
    // No further access to the object's attributes is permitted here.
  }
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    OnCheckBrowseUrlResult(const GURL& url,
                           safe_browsing::SBThreatType threat_type,
                           const safe_browsing::ThreatMetadata& metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Stop the timer to avoid `OnCheckBlocklistTimeout` from being called, since
  // we got a blocklist check result in time.
  timer_.Stop();
  if (threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    abusive_origins_->insert(url.spec());
  }
  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    OnCheckBlocklistTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  database_manager_->CancelCheck(this);
  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::PerformSafeBrowsingChecks(
    GURL url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto new_sb_check = std::make_unique<SafeBrowsingCheckClient>(
      database_manager_.get(), &abusive_notification_permission_origins_,
      &safe_browsing_request_clients_, url, safe_browsing_check_delay_);
  auto new_sb_check_ptr = new_sb_check.get();
  safe_browsing_request_clients_[new_sb_check_ptr] = std::move(new_sb_check);
  new_sb_check_ptr->CheckSocialEngineeringBlocklist();
}

bool AbusiveNotificationPermissionsManager::ShouldCheckOrigin(
    const ContentSettingPatternSource& setting) const {
  DCHECK(hcsm_);
  if (setting.setting_value == CONTENT_SETTING_ALLOW &&
      setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
    content_settings::SettingInfo info;
    // Since we are dealing with permissions for specific origins, and there
    // are no wildcard values in the pattern, it is safe to convert between
    // ContentSettingsPattern, string, and URL types.
    GURL setting_url(setting.primary_pattern.ToString());
    base::Value stored_value(hcsm_->GetWebsiteSetting(
        setting_url, setting_url,
        ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS, &info));

    if (!stored_value.is_dict()) {
      return false;
    }

    if (stored_value.GetDict()
            .Find(safety_hub::kRevokedStatusDictKeyStr)
            ->GetString() == safety_hub::kRevokeStr) {
      return true;
    }
    DCHECK(stored_value.GetDict()
               .Find(safety_hub::kRevokedStatusDictKeyStr)
               ->GetString() == safety_hub::kIgnoreStr);
  }
  return false;
}

void AbusiveNotificationPermissionsManager::ResetSafeBrowsingCheckHelpers() {
  abusive_notification_permission_origins_ = {};
  if (!safe_browsing_request_clients_.empty()) {
    safe_browsing_request_clients_.clear();
  }
}
