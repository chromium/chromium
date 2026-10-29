// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/permissions/permission_revocation_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
// Histogram names.
constexpr char kAbusiveNotificationPermissionRevocationHistogram[] =
    "Settings.SafetyHub.AbusiveNotificationPermissionRevocation";
constexpr char kPermissionChangedHistogramSuffix[] = "PermissionChanged";

void UpdateNotificationPermission(HostContentSettingsMap* hcsm,
                                  GURL url,
                                  ContentSetting setting_value) {
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      setting_value);
}

void RecordAbusiveNotificationPermissionChangedHistogram(
    bool is_ignored,
    safe_browsing::NotificationRevocationSource revocation_source,
    ContentSetting setting_value) {
  std::string_view revoke_status = is_ignored ? "Ignored" : "Revoked";
  std::string_view source_str;
  switch (revocation_source) {
    case safe_browsing::NotificationRevocationSource::
        kSocialEngineeringBlocklist:
      source_str = "SocialEngineeringBlocklist";
      break;
    case safe_browsing::NotificationRevocationSource::
        kSafeBrowsingUnwantedRevocation:
      source_str = "ManualSafeBrowsingRevocation";
      break;
    case safe_browsing::NotificationRevocationSource::
        kSuspiciousContentAutoRevocation:
      source_str = "SuspiciousContentAutoRevocation";
      break;
    default:
      source_str = "Unknown";
  }
  base::UmaHistogramEnumeration(
      base::StrCat({kAbusiveNotificationPermissionRevocationHistogram, ".",
                    source_str, ".", revoke_status, ".",
                    kPermissionChangedHistogramSuffix}),
      setting_value, ContentSetting::CONTENT_SETTING_NUM_SETTINGS);
}

// Convert string representation for storing in
// `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` into
// `NotificationRevocationSource`.
safe_browsing::NotificationRevocationSource GetNotificationRevocationSource(
    std::string source_str) {
  if (source_str == kSocialEngineeringBlocklistStr) {
    return safe_browsing::NotificationRevocationSource::
        kSocialEngineeringBlocklist;
  }
  if (source_str == kSafeBrowsingUnwantedRevocationStr) {
    return safe_browsing::NotificationRevocationSource::
        kSafeBrowsingUnwantedRevocation;
  }
  if (source_str == kSuspiciousContentAutoRevocationStr) {
    return safe_browsing::NotificationRevocationSource::
        kSuspiciousContentAutoRevocation;
  }
  // Only `kSocialEngineeringBlocklist` and `kSafeBrowsingUnwantedRevocation`
  // are stored in `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`, other type of
  // `NotificationRevocationSource` should never be the reason for abusive
  // notification revocation.
  return safe_browsing::NotificationRevocationSource::kUnknown;
}

// Return true if users has chosen to see original content of warned
// suspicious notification.
bool HasShowOriginalSuspiciousNotification(HostContentSettingsMap* hcsm,
                                           GURL url) {
  CHECK(hcsm, base::NotFatalUntil::M162);
  CHECK(url.is_valid(), base::NotFatalUntil::M162);
  base::Value stored_value(hcsm->GetWebsiteSetting(
      url, url, ContentSettingsType::SUSPICIOUS_NOTIFICATION_SHOW_ORIGINAL));

  if (stored_value.is_none()) {
    return false;
  }
  CHECK(stored_value.is_dict(), base::NotFatalUntil::M162);
  CHECK(stored_value.GetDict().contains(
            safe_browsing::kSuspiciousNotificationShowOriginalKey),
        base::NotFatalUntil::M162);
  return stored_value.GetDict()
      .FindBool(safe_browsing::kSuspiciousNotificationShowOriginalKey)
      .value_or(false);
}

// Return true if the url should be considered for suspicious content
// auto-revocation. This includes checking if the notification is enabled, no
// other abusive revocation has been done, user engagement with the site is
// sufficiently low, and user have not interacted with warning or re-granting
// permission for `url` previously.
bool ShouldCheckSuspiciousContentRevocationThreshold(
    Profile* profile,
    HostContentSettingsMap* hcsm,
    GURL url) {
  CHECK(hcsm, base::NotFatalUntil::M162);
  CHECK(url.is_valid(), base::NotFatalUntil::M162);
  // Notification permission for the site is not enabled or is not controlled by
  // the user (e.g. policy and extensions).
  if (hcsm->GetUserModifiableContentSetting(
          url, url, ContentSettingsType::NOTIFICATIONS) !=
      ContentSetting::CONTENT_SETTING_ALLOW) {
    return false;
  }
  // Notification permission has already been revoked for other abusive reasons.
  if (safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, url)) {
    return false;
  }
  // The user has previously re-granted revoked notification permission.
  if (safety_hub_util::IsAbusiveNotificationRevocationIgnored(hcsm, url) ||
      DisruptiveNotificationPermissionsManager::
          IsUrlIgnoredForRevokedDisruptiveNotification(hcsm, url)) {
    return false;
  }
  // The user interacts with suspicious notification warning to show original
  // content.
  if (HasShowOriginalSuspiciousNotification(hcsm, url)) {
    return false;
  }
  // Check site engagement score.
  auto* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile);
  if (!site_engagement_service) {
    return false;
  }
  double engagement_score = site_engagement_service->GetScore(url);
  // The user engagement with the site is higher than revocation threshold.
  if (engagement_score >=
      safe_browsing::kAutoRevokeSuspiciousNotificationEngagementScoreCutOff
          .Get()) {
    return false;
  }
  return true;
}
}  // namespace

AbusiveNotificationPermissionsManager::AbusiveNotificationPermissionsManager(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
        v5_get_hash_protocol_manager,
    scoped_refptr<HostContentSettingsMap> hcsm,
    PrefService* pref_service)
    : database_manager_(database_manager),
      hcsm_(hcsm),
      pref_service_(pref_service),
      v5_get_hash_protocol_manager_(v5_get_hash_protocol_manager),
      safe_browsing_check_delay_(kCheckUrlTimeoutMs) {}

AbusiveNotificationPermissionsManager::
    ~AbusiveNotificationPermissionsManager() = default;

// static
void AbusiveNotificationPermissionsManager::
    ExecuteAbusiveNotificationAutoRevocation(
        HostContentSettingsMap* hcsm,
        GURL url,
        safe_browsing::NotificationRevocationSource revocation_source,
        const raw_ptr<const base::Clock> clock) {
  UpdateNotificationPermission(hcsm, url,
                               ContentSetting::CONTENT_SETTING_DEFAULT);
  // Set the default constraint to the current time and lifetime defined by
  // the clean up threshold. Use this to set the expiration time of the
  // revocation permission.
  content_settings::ContentSettingConstraints default_constraint(clock->Now());
  default_constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
  SetRevokedAbusiveNotificationPermission(
      hcsm, url, /*is_ignored=*/false, revocation_source, default_constraint);
  content_settings_uma_util::RecordContentSettingsHistogram(
      "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2",
      ContentSettingsType::NOTIFICATIONS);
}

// static
void AbusiveNotificationPermissionsManager::
    SetRevokedAbusiveNotificationPermission(
        HostContentSettingsMap* hcsm,
        GURL url,
        bool is_ignored,
        safe_browsing::NotificationRevocationSource revocation_source,
        const content_settings::ContentSettingConstraints& constraints) {
  CHECK(url.is_valid(), base::NotFatalUntil::M162);
  // If the `url` should be ignore during future auto revocation, then the
  // constraint should not expire. If the lifetime is zero, then the setting
  // does not expire.
  if (is_ignored) {
    CHECK(constraints.lifetime().is_zero(), base::NotFatalUntil::M162);
    CHECK(constraints.expiration() == base::Time(), base::NotFatalUntil::M162);
    PermissionRevocationRequest::ExemptOriginFromFutureRevocations(hcsm, url);
  } else {
    PermissionRevocationRequest::UndoExemptOriginFromFutureRevocations(hcsm,
                                                                       url);
  }
  base::DictValue revoked_value;
  revoked_value.Set(
      safety_hub::kRevokedStatusDictKeyStr,
      is_ignored ? safety_hub::kIgnoreStr : safety_hub::kRevokeStr);
  std::optional<std::string> source_str =
      GetRevocationSourceString(revocation_source);
  if (source_str) {
    revoked_value.Set(kAbusiveRevocationSourceKeyStr, source_str.value());
  }
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(revoked_value)), constraints);
}

// static
// `SetRevokedAbusiveNotificationPermission`, preserving `revocation_source` if
// it exists. This method should be used for re-grant, undo of the re-grant, or
// other scenarios where there may be an existing revocation entry for the url.
void AbusiveNotificationPermissionsManager::
    SetRevokedAbusiveNotificationPermission(
        HostContentSettingsMap* hcsm,
        GURL url,
        bool is_ignored,
        const content_settings::ContentSettingConstraints& constraints) {
  safe_browsing::NotificationRevocationSource revocation_source =
      GetRevokedAbusiveNotificationRevocationSource(hcsm, url);

  SetRevokedAbusiveNotificationPermission(hcsm, url, is_ignored,
                                          revocation_source, constraints);
}

// static
bool AbusiveNotificationPermissionsManager::
    MaybeRevokeSuspiciousNotificationPermission(Profile* profile, GURL url) {
  CHECK(base::FeatureList::IsEnabled(
            safe_browsing::kAutoRevokeSuspiciousNotification),
        base::NotFatalUntil::M162);
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!url.is_valid() || !hcsm) {
    return false;
  }
  if (!ShouldCheckSuspiciousContentRevocationThreshold(profile, hcsm, url)) {
    return false;
  }

  // Check suspicious notification count.
  base::Value stored_value = hcsm->GetWebsiteSetting(
      url, url, ContentSettingsType::NOTIFICATION_INTERACTIONS);
  if (stored_value.is_none() || !stored_value.is_dict()) {
    return false;
  }
  int suspicious_notification_count = permissions::
      NotificationsEngagementService::GetSuspiciousNotificationCountForPeriod(
          stored_value.GetDict(),
          safe_browsing::kAutoRevokeSuspiciousNotificationLookBackPeriod.Get());
  // The `suspicious_notification_count` value represents how many suspicious
  // notification warnings have been sent to the user prior to the current. This
  // method is called when a notification should be replaced by a warning,
  // revoking permissions if this notification warning would put the count over
  // the threshold for revocation.
  if (suspicious_notification_count <
      safe_browsing::kAutoRevokeSuspiciousNotificationMinNotificationCount
          .Get()) {
    return false;
  }
  ExecuteAbusiveNotificationAutoRevocation(
      hcsm, url,
      safe_browsing::NotificationRevocationSource::
          kSuspiciousContentAutoRevocation,
      base::DefaultClock::GetInstance());
  base::UmaHistogramEnumeration("SafeBrowsing.NotificationRevocationSource",
                                safe_browsing::NotificationRevocationSource::
                                    kSuspiciousContentAutoRevocation);
  if (auto* notification_manager =
          RevokedPermissionsOSNotificationDisplayManagerFactory::GetForProfile(
              profile)) {
    notification_manager->DisplayNotification();
  }
  return true;
}

// static
safe_browsing::NotificationRevocationSource
AbusiveNotificationPermissionsManager::
    GetRevokedAbusiveNotificationRevocationSource(HostContentSettingsMap* hcsm,
                                                  GURL setting_url) {
  CHECK(setting_url.is_valid(), base::NotFatalUntil::M162);
  base::Value stored_value =
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm, setting_url);
  if (stored_value.is_none()) {
    return safe_browsing::NotificationRevocationSource::kUnknown;
  }
  const std::string* source_str =
      stored_value.GetDict().FindString(kAbusiveRevocationSourceKeyStr);
  if (!source_str) {
    return safe_browsing::NotificationRevocationSource::kUnknown;
  }
  return GetNotificationRevocationSource(*source_str);
}

// static
bool AbusiveNotificationPermissionsManager::IsUrlRevokedDueToSuspiciousContent(
    HostContentSettingsMap* hcsm,
    GURL setting_url) {
  CHECK(setting_url.is_valid(), base::NotFatalUntil::M162);
  return GetRevokedAbusiveNotificationRevocationSource(hcsm, setting_url) ==
         safe_browsing::NotificationRevocationSource::
             kSuspiciousContentAutoRevocation;
}

void AbusiveNotificationPermissionsManager::
    CheckNotificationPermissionOrigins() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
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

void AbusiveNotificationPermissionsManager::RegrantPermissionForOrigin(
    const url::Origin& origin) {
  // If the user decides to regrant permissions for `origin`, check if it has
  // revoked abusive notification permissions. If so, allow notification
  // permissions and ignore the `origin` from future auto-revocation.
  GURL url = origin.GetURL();
  CHECK(url.is_valid());
  if (!safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm_.get(), url)) {
    return;
  }
  safe_browsing::NotificationRevocationSource revocation_source =
      GetRevokedAbusiveNotificationRevocationSource(hcsm_.get(), url);
  UpdateNotificationPermissionForSafetyHubAction(
      hcsm_.get(), url, ContentSetting::CONTENT_SETTING_ALLOW);
  SetRevokedAbusiveNotificationPermission(hcsm_.get(), url,
                                          /*is_ignored=*/true,
                                          revocation_source);

  LogAbusiveNotificationPermissionRevocationUKM(
      url, AbusiveNotificationPermissionsInteractions::kAllowAgain,
      revocation_source);
}

void AbusiveNotificationPermissionsManager::UndoRegrantPermissionForOrigin(
    const url::Origin& origin,
    std::set<ContentSettingsType> permission_types,
    content_settings::ContentSettingConstraints constraints) {
  // The user has decided to undo the regranted permission revocation for
  // `origin`. Only update the `NOTIFICATIONS` and
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings if the origin had
  // revoked notification permissions.
  if (!permission_types.contains(ContentSettingsType::NOTIFICATIONS)) {
    return;
  }
  GURL url = origin.GetURL();
  CHECK(url.is_valid());
  base::Value stored_value(hcsm_->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS));
  if (stored_value.is_none()) {
    return;
  }
  safe_browsing::NotificationRevocationSource revocation_source =
      GetRevokedAbusiveNotificationRevocationSource(hcsm_.get(), url);
  UpdateNotificationPermissionForSafetyHubAction(
      hcsm_.get(), url, ContentSetting::CONTENT_SETTING_DEFAULT);
  SetRevokedAbusiveNotificationPermission(hcsm_.get(), url,
                                          /*is_ignored=*/false,
                                          revocation_source, constraints);

  LogAbusiveNotificationPermissionRevocationUKM(
      url, AbusiveNotificationPermissionsInteractions::kUndoAllowAgain,
      revocation_source);
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

void AbusiveNotificationPermissionsManager::OnPermissionChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  GURL setting_url = primary_pattern.ToRepresentativeUrl();
  if (!setting_url.is_valid()) {
    return;
  }
  base::Value stored_value =
      safety_hub_util::GetRevokedAbusiveNotificationPermissionsSettingValue(
          hcsm_.get(), setting_url);
  if (stored_value.is_none()) {
    // This permission change is unrelated to abusive revocation; do nothing.
    return;
  }
  bool is_ignored = safety_hub_util::IsAbusiveNotificationRevocationIgnored(
      hcsm_.get(), setting_url);
  safe_browsing::NotificationRevocationSource revocation_source =
      GetRevokedAbusiveNotificationRevocationSource(hcsm_.get(), setting_url);
  ContentSetting new_setting = hcsm_->GetContentSetting(
      setting_url, secondary_pattern.ToRepresentativeUrl(),
      ContentSettingsType::NOTIFICATIONS);
  RecordAbusiveNotificationPermissionChangedHistogram(
      is_ignored, revocation_source, new_setting);

  // If the user re-granted a notification permission that was auto-revoked,
  // set revocation entry to ignore so the permission will not be
  // auto-revoked again.
  if (new_setting == CONTENT_SETTING_ALLOW &&
      (revocation_source == safe_browsing::NotificationRevocationSource::
                                kSuspiciousContentAutoRevocation ||
       revocation_source == safe_browsing::NotificationRevocationSource::
                                kSocialEngineeringBlocklist ||
       revocation_source == safe_browsing::NotificationRevocationSource::
                                kSafeBrowsingUnwantedRevocation)) {
    SetRevokedAbusiveNotificationPermission(hcsm_.get(), setting_url,
                                            /*is_ignored=*/true);
    // Return without deleting abusive revocation entry.
    return;
  }
  // Delete entry from abusive notification list as we assume the user is taking
  // an active decision on the revocation. Note removal of entry with revoked
  // status "ignored" will result in notification being auto-revoked again once
  // criteria are met.
  DeletePatternFromRevokedAbusiveNotificationList(primary_pattern,
                                                  secondary_pattern);
}

void AbusiveNotificationPermissionsManager::RestoreDeletedRevokedPermission(
    const ContentSettingsPattern& primary_pattern,
    content_settings::ContentSettingConstraints constraints) {
  SetRevokedAbusiveNotificationPermission(hcsm_.get(),
                                          primary_pattern.ToRepresentativeUrl(),
                                          /*is_ignored=*/false, constraints);
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

void AbusiveNotificationPermissionsManager::
    LogAbusiveNotificationPermissionRevocationUKM(
        const GURL& origin,
        AbusiveNotificationPermissionsInteractions interaction,
        safe_browsing::NotificationRevocationSource revocation_source) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForNotificationEvent(
      base::PassKey<AbusiveNotificationPermissionsManager>(), origin);
  ukm::builders::SafetyHub_AbusiveNotificationPermissionRevocation_Interactions(
      source_id)
      .SetInteractionType(static_cast<int>(interaction))
      .SetRevocationSource(static_cast<int>(revocation_source))
      .Record(ukm::UkmRecorder::Get());
}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    SafeBrowsingCheckClient(
        base::PassKey<safe_browsing::SafeBrowsingDatabaseManager::Client>
            pass_key,
        safe_browsing::SafeBrowsingDatabaseManager* database_manager,
        base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
            v5_get_hash_protocol_manager,
        raw_ptr<std::map<SafeBrowsingCheckClient*,
                         std::unique_ptr<SafeBrowsingCheckClient>>>
            safe_browsing_request_clients,
        raw_ptr<HostContentSettingsMap> hcsm,
        PrefService* pref_service,
        GURL url,
        int safe_browsing_check_delay,
        const base::Clock* clock)
    : safe_browsing::SafeBrowsingDatabaseManager::Client(std::move(pass_key)),
      database_manager_(database_manager),
      v5_get_hash_protocol_manager_(v5_get_hash_protocol_manager),
      safe_browsing_request_clients_(safe_browsing_request_clients),
      hcsm_(hcsm),
      pref_service_(pref_service),
      url_(url),
      safe_browsing_check_delay_(safe_browsing_check_delay),
      clock_(clock) {}

AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    ~SafeBrowsingCheckClient() {
  if (timer_.IsRunning()) {
    CHECK(database_manager_, base::NotFatalUntil::M162);
    database_manager_->CancelCheck(this);
    timer_.Stop();
  }
}

base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    GetV5GetHashProtocolManager() {
  return v5_get_hash_protocol_manager_;
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    CheckSocialEngineeringBlocklist() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
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
  CHECK(database_manager_, base::NotFatalUntil::M162);
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
    LogCheckResult(CheckResult::kSafe);
    safe_browsing_request_clients_->erase(this);
    // The previous line results in deleting this object.
    // No further access to the object's attributes is permitted here.
  }
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    OnCheckBrowseUrlResult(const GURL& url,
                           safe_browsing::SBThreatType threat_type) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  // Stop the timer to avoid `OnCheckBlocklistTimeout` from being called, since
  // we got a blocklist check result in time.
  timer_.Stop();
  LogCheckResult(
      threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING
          ? CheckResult::kPhishing
          : CheckResult::kSafe);
  if (threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    ExecuteAbusiveNotificationAutoRevocation(
        hcsm_.get(), url,
        safe_browsing::NotificationRevocationSource::
            kSocialEngineeringBlocklist,
        clock_);
    safe_browsing::SafeBrowsingMetricsCollector::
        LogSafeBrowsingNotificationRevocationSourceHistogram(
            safe_browsing::NotificationRevocationSource::
                kSocialEngineeringBlocklist);
  }
  // Update user pref that stores the time of the last successful blocklist
  // check.
  if (pref_service_) {
    base::TimeDelta delta_since_unix_epoch =
        base::Time::Now() - base::Time::UnixEpoch();
    pref_service_->SetInt64(
        safety_hub_prefs::
            kLastTimeInMsAbusiveNotificationBlocklistCheckCompleted,
        delta_since_unix_epoch.InMilliseconds());
  }

  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    OnCheckBlocklistTimeout() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(database_manager_, base::NotFatalUntil::M162);
  database_manager_->CancelCheck(this);
  LogCheckResult(CheckResult::kTimeout);
  safe_browsing_request_clients_->erase(this);
  // The previous line results in deleting this object.
  // No further access to the object's attributes is permitted here.
}

void AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient::
    LogCheckResult(CheckResult result) {
  base::UmaHistogramEnumeration(
      "Settings.SafetyHub.AbusiveNotificationPermissionRevocation.CheckResult",
      result);
}

void AbusiveNotificationPermissionsManager::PerformSafeBrowsingChecks(
    GURL url) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(database_manager_, base::NotFatalUntil::M162);
  auto new_sb_check = std::make_unique<SafeBrowsingCheckClient>(
      safe_browsing::SafeBrowsingDatabaseManager::Client::GetPassKey(),
      database_manager_.get(), v5_get_hash_protocol_manager_,
      &safe_browsing_request_clients_, hcsm_.get(), pref_service_, url,
      safe_browsing_check_delay_, GetClock());
  auto new_sb_check_ptr = new_sb_check.get();
  safe_browsing_request_clients_[new_sb_check_ptr] = std::move(new_sb_check);
  new_sb_check_ptr->CheckSocialEngineeringBlocklist();
}

bool AbusiveNotificationPermissionsManager::ShouldCheckOrigin(
    const ContentSettingPatternSource& setting) const {
  CHECK(hcsm_, base::NotFatalUntil::M162);
  // Skip wildcard patterns that don't belong to a single origin.
  if (!setting.primary_pattern.MatchesSingleOrigin()) {
    return false;
  }
  // Skip file:// permissions.
  if (setting.primary_pattern.GetSchemeType() ==
      ContentSettingsPattern::SCHEME_FILE) {
    return false;
  }
  // Skip checks when they've already been performed within the last 24 hours.
  if (pref_service_) {
    base::TimeDelta delta_since_unix_epoch =
        base::Time::Now() - base::Time::UnixEpoch();
    base::TimeDelta last_check_time =
        base::Milliseconds(pref_service_->GetInt64(
            safety_hub_prefs::
                kLastTimeInMsAbusiveNotificationBlocklistCheckCompleted));
    // If a previous check has occurred and was within the last day, skip
    // checks.
    if (last_check_time > base::Milliseconds(0) &&
        delta_since_unix_epoch - last_check_time < base::Days(1)) {
      return false;
    }
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
    // Track how often we hit this desynced state (where the setting is ALLOW
    // but the revocation status is not marked as ignore).
    bool is_ignored = safety_hub_util::IsAbusiveNotificationRevocationIgnored(
        hcsm_.get(), setting.primary_pattern.ToRepresentativeUrl());
    base::UmaHistogramBoolean(
        "Settings.SafetyHub.AbusiveNotificationPermissionRevocation."
        "InconsistentStateEncountered",
        !is_ignored);
    // If the url has a REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS setting value
    // and the NOTIFICATIONS permission is set to CONTENT_SETTING_ALLOW, then
    // the user chose to ignore the origin for future revocations. This could have been done
    // by re-allowing in Safety Hub or otherwise manually updating the notification permission
    // following an auto-revocation. We do not need to check the origin again.
    return false;
  }
  return false;
}

void AbusiveNotificationPermissionsManager::ResetSafeBrowsingCheckHelpers() {
  if (!safe_browsing_request_clients_.empty()) {
    safe_browsing_request_clients_.clear();
  }
}

void AbusiveNotificationPermissionsManager::
    UpdateNotificationPermissionForSafetyHubAction(
        HostContentSettingsMap* hcsm,
        GURL url,
        ContentSetting setting_value) {
  base::AutoReset<bool> is_abusive_site_revocation_running(
      &is_abusive_site_revocation_running_, true);
  UpdateNotificationPermission(hcsm, url, setting_value);
}

// static
std::optional<std::string>
AbusiveNotificationPermissionsManager::GetRevocationSourceString(
    safe_browsing::NotificationRevocationSource source) {
  switch (source) {
    case safe_browsing::NotificationRevocationSource::
        kSocialEngineeringBlocklist:
      return kSocialEngineeringBlocklistStr;
    case safe_browsing::NotificationRevocationSource::
        kSafeBrowsingUnwantedRevocation:
      return kSafeBrowsingUnwantedRevocationStr;
    case safe_browsing::NotificationRevocationSource::
        kSuspiciousContentAutoRevocation:
      return kSuspiciousContentAutoRevocationStr;
    // Other revocation sources are not stored in
    // REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS setting.
    default:
      return std::nullopt;
  }
}
