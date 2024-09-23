// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/common/features.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

void UpdateNotificationPermission(HostContentSettingsMap* hcsm,
                                  GURL url,
                                  ContentSetting setting_value) {
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      setting_value);
}

}  // namespace

AbusiveNotificationPermissionsManager::AbusiveNotificationPermissionsManager(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<HostContentSettingsMap> hcsm)
    : database_manager_(database_manager),
      hcsm_(hcsm),
      safe_browsing_check_delay_(kCheckUrlTimeoutMs) {}

AbusiveNotificationPermissionsManager::
    ~AbusiveNotificationPermissionsManager() = default;

void AbusiveNotificationPermissionsManager::
    CheckNotificationPermissionOrigins() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!database_manager_) {
    return;
  }
  ResetSafeBrowsingCheckHelpers();
  // Keep track of blocklist check count for logging histogram below.
  int blocklist_check_counter = 0;
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
      blocklist_check_counter += 1;
    }
  }
  base::UmaHistogramCounts100(safety_hub::kBlocklistCheckCountHistogramName,
                              blocklist_check_counter);
}

void AbusiveNotificationPermissionsManager::
    RegrantPermissionForOriginIfNecessary(const GURL& url) {
  // If the user decides to regrant permissions for `url`, check if it has
  // revoked abusive notification permissions. If so, allow notification
  // permissions and ignore the `url` from future auto-revocation.
  if (!safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm_.get(), url)) {
    return;
  }
  // Set this to true to prevent removal of revoked setting values.
  is_abusive_site_revocation_running_ = true;
  UpdateNotificationPermission(hcsm_.get(), url,
                               ContentSetting::CONTENT_SETTING_ALLOW);
  safety_hub_util::SetRevokedAbusiveNotificationPermission(hcsm_.get(), url,
                                                           /*is_ignored=*/true);
  // Set this back to false, so that revoked settings can be cleaned up if
  // necessary.
  is_abusive_site_revocation_running_ = false;
}

void AbusiveNotificationPermissionsManager::
    UndoRegrantPermissionForOriginIfNecessary(
        const GURL& url,
        std::set<ContentSettingsType> permission_types,
        content_settings::ContentSettingConstraints constraints) {
  // The user has decided to undo the regranted permission revocation for `url`.
  // Only update the `NOTIFICATIONS` and
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings if the url had revoked
  // notification permissions.
  if (!permission_types.contains(ContentSettingsType::NOTIFICATIONS)) {
    return;
  }
  // Set this to true to prevent removal of revoked setting values.
  is_abusive_site_revocation_running_ = true;
  UpdateNotificationPermission(hcsm_.get(), url,
                               ContentSetting::CONTENT_SETTING_DEFAULT);
  safety_hub_util::SetRevokedAbusiveNotificationPermission(
      hcsm_.get(), url, /*is_ignored=*/false, constraints);
  // Set this back to false, so that revoked settings can be cleaned up if
  // necessary.
  is_abusive_site_revocation_running_ = false;
}

void AbusiveNotificationPermissionsManager::ClearRevokedPermissionsList() {
  ContentSettingsForOneType revoked_permissions =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm_.get());
  for (const auto& revoked_permission : revoked_permissions) {
    DeletePatternFromRevokedAbusiveNotificationList(
        revoked_permission.primary_pattern,
        revoked_permission.secondary_pattern);
  }
}

void AbusiveNotificationPermissionsManager::
    DeletePatternFromRevokedAbusiveNotificationList(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS, {});
}

const base::Clock* AbusiveNotificationPermissionsManager::GetClock() {
  if (clock_for_testing_) {
    return clock_for_testing_;
  }
  return base::DefaultClock::GetInstance();
}

bool AbusiveNotificationPermissionsManager::IsRevocationRunning() {
  return is_abusive_site_revocation_running_ ||
         !safe_browsing_request_clients_.empty();
}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    SafeBrowsingCheckClient(
        safe_browsing::SafeBrowsingDatabaseManager* database_manager,
        raw_ptr<std::map<SafeBrowsingCheckClient*,
                         std::unique_ptr<SafeBrowsingCheckClient>>>
            safe_browsing_request_clients,
        raw_ptr<HostContentSettingsMap> hcsm,
        GURL url,
        int safe_browsing_check_delay,
        const base::Clock* clock)
    : database_manager_(database_manager),
      safe_browsing_request_clients_(safe_browsing_request_clients),
      hcsm_(hcsm),
      url_(url),
      safe_browsing_check_delay_(safe_browsing_check_delay),
      clock_(clock) {}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    ~SafeBrowsingCheckClient() {
  if (timer_.IsRunning()) {
    DCHECK(database_manager_);
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
  DCHECK(database_manager_);
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
    UpdateNotificationPermission(hcsm_.get(), url,
                                 ContentSetting::CONTENT_SETTING_DEFAULT);
    content_settings::ContentSettingConstraints default_constraint(
        clock_->Now());
    default_constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
    safety_hub_util::SetRevokedAbusiveNotificationPermission(
        hcsm_.get(), url, /*is_ignored=*/false, default_constraint);
    content_settings_uma_util::RecordContentSettingsHistogram(
        "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2",
        ContentSettingsType::NOTIFICATIONS);
  }
  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    OnCheckBlocklistTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(database_manager_);
  database_manager_->CancelCheck(this);
  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::PerformSafeBrowsingChecks(
    GURL url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(database_manager_);
  auto new_sb_check = std::make_unique<SafeBrowsingCheckClient>(
      database_manager_.get(), &safe_browsing_request_clients_, hcsm_.get(),
      url, safe_browsing_check_delay_, GetClock());
  auto new_sb_check_ptr = new_sb_check.get();
  safe_browsing_request_clients_[new_sb_check_ptr] = std::move(new_sb_check);
  new_sb_check_ptr->CheckSocialEngineeringBlocklist();
}

bool AbusiveNotificationPermissionsManager::ShouldCheckOrigin(
    const ContentSettingPatternSource& setting) const {
  DCHECK(hcsm_);
  // Skip wildcard patterns that don't belong to a single origin.
  if (!setting.primary_pattern.MatchesSingleOrigin()) {
    return false;
  }
  if (setting.setting_value == CONTENT_SETTING_ALLOW) {
    // Secondary pattern should be wildcard for notification permissions. If
    // not, the permission should be ignored.
    if (setting.secondary_pattern != ContentSettingsPattern::Wildcard()) {
      return false;
    }
    // If the url is not valid, do not check the origin.
    GURL setting_url = setting.primary_pattern.ToRepresentativeUrl();
    if (!setting_url.is_valid()) {
      return false;
    }
    // If the url does not have a REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS
    // setting value, we should check the origin.
    base::Value stored_value =
        safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
            hcsm_.get(), setting_url);
    if (stored_value.is_none()) {
      return true;
    }
    // If the url has a REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS setting value
    // and the NOTIFICATIONS permission is set to CONTENT_SETTING_ALLOW, then
    // the user chose to ignore the origin for future revocations so the setting
    // value should specify ignore.
    DCHECK(safety_hub_util::IsAbusiveNotificationRevocationIgnored(
        hcsm_.get(), setting.primary_pattern.ToRepresentativeUrl()));
    return false;
  }
  return false;
}

void AbusiveNotificationPermissionsManager::ResetSafeBrowsingCheckHelpers() {
  if (!safe_browsing_request_clients_.empty()) {
    safe_browsing_request_clients_.clear();
  }
}
