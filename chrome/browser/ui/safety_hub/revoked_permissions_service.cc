// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safety_check/safety_check.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page.h"
#include "revoked_permissions_service.h"
#include "url/origin.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

namespace {

// Determines the frequency at which permissions of sites are checked whether
// they are unused.
const base::TimeDelta kUnusedSitePermissionsRepeatedUpdateInterval =
    base::Days(1);

content_settings::ContentSettingConstraints GetConstraintFromInfo(
    const content_settings::SettingInfo& info) {
  auto constraint = content_settings::ContentSettingConstraints(
      info.metadata.expiration() - info.metadata.lifetime());
  constraint.set_lifetime(info.metadata.lifetime());
  return constraint;
}

bool IsUnusedPermissionRevocation(PermissionsRevocationType revocation_type) {
  return revocation_type == PermissionsRevocationType::kUnusedPermissions ||
         revocation_type == PermissionsRevocationType::
                                kUnusedPermissionsAndAbusiveNotifications ||
         revocation_type == PermissionsRevocationType::
                                kUnusedPermissionsAndDisruptiveNotifications;
}

bool IsAbusiveNotificationPermissionRevocation(
    PermissionsRevocationType revocation_type) {
  return revocation_type ==
             PermissionsRevocationType::kAbusiveNotificationPermissions ||
         revocation_type == PermissionsRevocationType::
                                kUnusedPermissionsAndAbusiveNotifications;
}

bool IsDisruptiveNotificationPermissionRevocation(
    PermissionsRevocationType revocation_type) {
  return revocation_type ==
             PermissionsRevocationType::kDisruptiveNotificationPermissions ||
         revocation_type == PermissionsRevocationType::
                                kUnusedPermissionsAndDisruptiveNotifications;
}

}  // namespace

base::TimeDelta RevokedPermissionsService::GetRepeatedUpdateInterval() {
  return kUnusedSitePermissionsRepeatedUpdateInterval;
}

RevokedPermissionsService::TabHelper::TabHelper(
    content::WebContents* web_contents,
    RevokedPermissionsService* unused_site_permission_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents),
      unused_site_permission_service_(
          unused_site_permission_service->AsWeakPtr()) {}

RevokedPermissionsService::TabHelper::~TabHelper() = default;

PermissionsData::PermissionsData() = default;

PermissionsData::~PermissionsData() = default;

PermissionsData::PermissionsData(const PermissionsData& other)
    : primary_pattern(other.primary_pattern),
      permission_types(other.permission_types),
      constraints(other.constraints.Clone()),
      revocation_type(other.revocation_type) {
  chooser_permissions_data = other.chooser_permissions_data.Clone();
}

void RevokedPermissionsService::TabHelper::PrimaryPageChanged(
    content::Page& page) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile, page.GetMainDocument().GetLastCommittedURL(),
      DisruptiveNotificationPermissionsManager::FalsePositiveReason::kPageVisit,
      page.GetMainDocument().GetPageUkmSourceId());

  if (unused_site_permission_service_) {
    unused_site_permission_service_->OnPageVisited(
        page.GetMainDocument().GetLastCommittedOrigin());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RevokedPermissionsService::TabHelper);

RevokedPermissionsService::RevokedPermissionsService(
    content::BrowserContext* browser_context,
    PrefService* prefs)
    : browser_context_(browser_context),
      clock_(base::DefaultClock::GetInstance()) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_context_);

  content_settings_observation_.Observe(hcsm());
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

#if BUILDFLAG(IS_ANDROID)
    pref_change_registrar_->Add(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled,
        base::BindRepeating(&RevokedPermissionsService::
                                OnPermissionsAutorevocationControlChanged,
                            base::Unretained(this)));
#else   // BUILDFLAG(IS_ANDROID)
  pref_change_registrar_->Add(
      safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled,
      base::BindRepeating(
          &RevokedPermissionsService::OnPermissionsAutorevocationControlChanged,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_ANDROID)

    RevokedPermissionsOSNotificationDisplayManager*
        notification_display_manager =
            RevokedPermissionsOSNotificationDisplayManagerFactory::
                GetForProfile(Profile::FromBrowserContext(browser_context_));
    abusive_notification_manager_ =
        std::make_unique<AbusiveNotificationPermissionsManager>(
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
            g_browser_process->safe_browsing_service()
                ? g_browser_process->safe_browsing_service()->database_manager()
                : nullptr,
#else
          nullptr,
#endif
            hcsm(), pref_change_registrar_->prefs());

  pref_change_registrar_->Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &RevokedPermissionsService::OnPermissionsAutorevocationControlChanged,
          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(
          features::kSafetyHubDisruptiveNotificationRevocation)) {
    disruptive_notification_manager_ =
        std::make_unique<DisruptiveNotificationPermissionsManager>(
            hcsm(),
            site_engagement::SiteEngagementServiceFactory::GetForProfile(
                browser_context_),
            notification_display_manager);
  }

  unused_site_permissions_manager_ =
      std::make_unique<UnusedSitePermissionsManager>(browser_context, prefs);

  InitializeLatestResult();

  if (IsUnusedSiteAutoRevocationEnabled() ||
      IsAbusiveNotificationAutoRevocationEnabled()) {
    hcsm()->EnsureSettingsUpToDate(
        base::BindOnce(&RevokedPermissionsService::MaybeStartRepeatedUpdates,
                       weak_factory_.GetWeakPtr()));
  }
}

RevokedPermissionsService::~RevokedPermissionsService() = default;

void RevokedPermissionsService::MaybeStartRepeatedUpdates() {
  if (IsUnusedSiteAutoRevocationEnabled() ||
      IsAbusiveNotificationAutoRevocationEnabled()) {
    StartRepeatedUpdates();
  }
}

std::unique_ptr<SafetyHubResult>
RevokedPermissionsService::InitializeLatestResultImpl() {
  return GetRevokedPermissions();
}

void RevokedPermissionsService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // When content settings change for a permissions that we might have
  // autorevoked, unless this happens because of autorevocation itself, we clean
  // it up, since we assume the user is taking an active decision on that
  // revocation (they are either changing settings or visiting the page and
  // reacting to a permission prompt). We handle notifications separately than
  // other permissions since they are revoked separately and treated separately
  // in Safety Check.

  if (content_type_set.ContainsAllTypes()) {
    // This only happens on initialization, so we do nothing.
    return;
  }

  const bool is_revocation_running =
      (unused_site_permissions_manager_ &&
       unused_site_permissions_manager_->IsRevocationRunning()) ||
      (IsAbusiveNotificationAutoRevocationEnabled() &&
       abusive_notification_manager_->IsRevocationRunning()) ||
      (disruptive_notification_manager_ &&
       disruptive_notification_manager_->IsChangingContentSettings());
  if (is_revocation_running) {
    return;
  }

  if (content_settings::IsPermissionEligibleForAutoRevocation(
          content_type_set.GetType())) {
    unused_site_permissions_manager_
        ->DeletePatternFromRevokedUnusedSitePermissionList(primary_pattern,
                                                           secondary_pattern);
  }

  if (content_type_set.GetType() == ContentSettingsType::NOTIFICATIONS) {
    // There should be at most one active revocation per site: either abusive or
    // disruptive.
    if (IsAbusiveNotificationAutoRevocationEnabled()) {
      abusive_notification_manager_->OnPermissionChanged(primary_pattern,
                                                         secondary_pattern);
    }
    if (disruptive_notification_manager_) {
      disruptive_notification_manager_->OnPermissionChanged(primary_pattern,
                                                            secondary_pattern);
    }
  }

  // Update OS notification to reflect changes in abusive or disruptive
  // revocations.
  if (content_type_set.Contains(
          ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS) ||
      content_type_set.Contains(
          ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS)) {
    RevokedPermissionsOSNotificationDisplayManager* manager =
        RevokedPermissionsOSNotificationDisplayManagerFactory::GetForProfile(
            Profile::FromBrowserContext(browser_context_));
    if (manager) {
      manager->UpdateNotification();
    }
  }
}

void RevokedPermissionsService::Shutdown() {
  content_settings_observation_.Reset();
}

void RevokedPermissionsService::RegrantPermissionsForOrigin(
    const url::Origin& origin) {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->RegrantPermissionForOriginIfNecessary(
        origin.GetURL());
  }

  if (disruptive_notification_manager_) {
    disruptive_notification_manager_->RegrantPermissionForUrl(origin.GetURL());
  }

  unused_site_permissions_manager_->RegrantPermissionsForOrigin(origin);
}

void RevokedPermissionsService::UndoRegrantPermissionsForOrigin(
    const PermissionsData& permissions_data) {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->UndoRegrantPermissionForOriginIfNecessary(
        GURL(permissions_data.primary_pattern.ToString()),
        permissions_data.permission_types,
        permissions_data.constraints.Clone());
  }

  if (disruptive_notification_manager_) {
    disruptive_notification_manager_->UndoRegrantPermissionForUrl(
        GURL(permissions_data.primary_pattern.ToString()),
        permissions_data.permission_types,
        permissions_data.constraints.Clone());
  }

  unused_site_permissions_manager_->UndoRegrantPermissionsForOrigin(
      permissions_data);
}

void RevokedPermissionsService::ClearRevokedPermissionsList() {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->ClearRevokedPermissionsList();
  }

  if (disruptive_notification_manager_) {
    disruptive_notification_manager_->ClearRevokedPermissionsList();
  }

  for (const auto& revoked_permissions : hcsm()->GetSettingsForOneType(
           ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)) {
    unused_site_permissions_manager_
        ->DeletePatternFromRevokedUnusedSitePermissionList(
            revoked_permissions.primary_pattern,
            revoked_permissions.secondary_pattern);
  }
}

// Called by TabHelper when a URL was visited.
void RevokedPermissionsService::OnPageVisited(const url::Origin& origin) {
  CHECK(unused_site_permissions_manager_);
  unused_site_permissions_manager_->OnPageVisited(origin);
}

base::OnceCallback<std::unique_ptr<SafetyHubResult>()>
RevokedPermissionsService::GetBackgroundTask() {
  return base::BindOnce(&UnusedSitePermissionsManager::UpdateOnBackgroundThread,
                        clock_, base::WrapRefCounted(hcsm()));
}

std::unique_ptr<SafetyHubResult> RevokedPermissionsService::UpdateOnUIThread(
    std::unique_ptr<SafetyHubResult> result) {
  if (IsUnusedSiteAutoRevocationEnabled()) {
    unused_site_permissions_manager_->RevokeUnusedPermissions(
        std::move(result));
    if (disruptive_notification_manager_) {
      disruptive_notification_manager_->RevokeDisruptiveNotifications();
    }
  }
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->CheckNotificationPermissionOrigins();
  }
  return GetRevokedPermissions();
}

std::unique_ptr<RevokedPermissionsResult>
RevokedPermissionsService::GetRevokedPermissions() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  auto result = std::make_unique<RevokedPermissionsResult>();

  for (const auto& revoked_permissions : settings) {
    PermissionsData permissions_data;
    permissions_data.primary_pattern = revoked_permissions.primary_pattern;
    const base::Value& stored_value = revoked_permissions.setting_value;
    CHECK(stored_value.is_dict());

    const base::Value::List* type_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    CHECK(type_list);
    for (const base::Value& type_value : *type_list) {
      // To avoid crashes for unknown types skip integer values.
      if (type_value.is_int()) {
        continue;
      }

      ContentSettingsType type =
          UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
              type_value.GetString());
      permissions_data.permission_types.insert(type);
    }

    permissions_data.constraints = content_settings::ContentSettingConstraints(
        revoked_permissions.metadata.expiration() -
        revoked_permissions.metadata.lifetime());
    permissions_data.constraints.set_lifetime(
        revoked_permissions.metadata.lifetime());

    auto* chooser_permissions_data_dict = stored_value.GetDict().FindDict(
        permissions::kRevokedChooserPermissionsKey);
    if (chooser_permissions_data_dict) {
      permissions_data.chooser_permissions_data =
          chooser_permissions_data_dict->Clone();
    }

    // If the origin has a revoked notification, add `NOTIFICATIONS` to
    // the list of revoked permissions.
    const GURL& url = GURL(revoked_permissions.primary_pattern.ToString());
    if (safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm(), url)) {
      CHECK(IsAbusiveNotificationAutoRevocationEnabled());
      permissions_data.permission_types.insert(
          static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));

      // Update `constraints` to one with the latest expiration.
      content_settings::SettingInfo info;
      base::Value stored_abusive_value(hcsm()->GetWebsiteSetting(
          url, url,
          ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
          &info));
      CHECK(!stored_abusive_value.is_none());
      if (revoked_permissions.metadata.expiration() <
          info.metadata.expiration()) {
        permissions_data.constraints = GetConstraintFromInfo(info);
      }
      // Suspicious content revocation is considered abusive notification
      // permission but revocation should be displayed with it own string
      // explanation.
      if (AbusiveNotificationPermissionsManager::
              IsUrlRevokedDueToSuspiciousContent(hcsm(), url)) {
        permissions_data.revocation_type = PermissionsRevocationType::
            kUnusedPermissionsAndSuspiciousNotifications;
      } else {
        permissions_data.revocation_type = PermissionsRevocationType::
            kUnusedPermissionsAndAbusiveNotifications;
      }

    } else if (DisruptiveNotificationPermissionsManager::
                   IsUrlRevokedDisruptiveNotification(hcsm(), url)) {
      // If the origin has a revoked disruptive notification, add
      // `NOTIFICATIONS` to the list of revoked permissions.
      CHECK(disruptive_notification_manager_);
      permissions_data.permission_types.insert(
          static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));
      // Update `constraints` to one with the latest expiration.
      content_settings::SettingInfo info;
      base::Value stored_disruptive_value(hcsm()->GetWebsiteSetting(
          url, url,
          ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
          &info));
      CHECK(!stored_disruptive_value.is_none());
      if (revoked_permissions.metadata.expiration() <
          info.metadata.expiration()) {
        permissions_data.constraints = GetConstraintFromInfo(info);
      }
      permissions_data.revocation_type = PermissionsRevocationType::
          kUnusedPermissionsAndDisruptiveNotifications;
    } else {
      permissions_data.revocation_type =
          PermissionsRevocationType::kUnusedPermissions;
    }

    result->AddRevokedPermission(permissions_data);
  }

  ContentSettingsForOneType revoked_abusive_notification_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  for (const auto& revoked_abusive_notification_permission :
       revoked_abusive_notification_settings) {
    const GURL& abusive_url = GURL(
        revoked_abusive_notification_permission.primary_pattern.ToString());
    // Skip origins with revoked unused site permissions, since these were
    // handled above.
    if (safety_hub_util::IsUrlRevokedUnusedSite(hcsm(), abusive_url)) {
      continue;
    }
    PermissionsData permissions_data;
    permissions_data.primary_pattern =
        revoked_abusive_notification_permission.primary_pattern;
    permissions_data.permission_types.insert(
        static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));

    permissions_data.constraints = content_settings::ContentSettingConstraints(
        revoked_abusive_notification_permission.metadata.expiration() -
        revoked_abusive_notification_permission.metadata.lifetime());
    permissions_data.constraints.set_lifetime(
        revoked_abusive_notification_permission.metadata.lifetime());

    if (AbusiveNotificationPermissionsManager::
            IsUrlRevokedDueToSuspiciousContent(hcsm(), abusive_url)) {
      permissions_data.revocation_type =
          PermissionsRevocationType::kSuspiciousNotificationPermissions;
    } else {
      permissions_data.revocation_type =
          PermissionsRevocationType::kAbusiveNotificationPermissions;
    }

    result->AddRevokedPermission(permissions_data);
  }

  if (disruptive_notification_manager_) {
    ContentSettingsForOneType revoked_disruptive_notifications =
        disruptive_notification_manager_->GetRevokedNotifications(hcsm());
    for (const auto& permission : revoked_disruptive_notifications) {
      // Skip origins with revoked unused site permissions, since these were
      // handled above.
      if (safety_hub_util::IsUrlRevokedUnusedSite(
              hcsm(), GURL(permission.primary_pattern.ToString()))) {
        continue;
      }
      // Skip origins with revoked abusive site permissions as these were
      // handled above. This is generally unlikely but it is possible if abusive
      // notification auto-revocation outside of Safety Hub was triggered in
      // between disruptive revocation run.
      if (safety_hub_util::IsUrlRevokedAbusiveNotification(
              hcsm(), GURL(permission.primary_pattern.ToString()))) {
        continue;
      }
      PermissionsData permissions_data;
      permissions_data.primary_pattern = permission.primary_pattern;
      permissions_data.permission_types.insert(
          static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));

      permissions_data.constraints =
          content_settings::ContentSettingConstraints(
              permission.metadata.expiration() -
              permission.metadata.lifetime());
      permissions_data.constraints.set_lifetime(permission.metadata.lifetime());

      permissions_data.revocation_type =
          PermissionsRevocationType::kDisruptiveNotificationPermissions;

      result->AddRevokedPermission(permissions_data);
    }
  }

  return result;
}

void RevokedPermissionsService::RestoreDeletedRevokedPermissionsList(
    const std::vector<PermissionsData>& permissions_data_list) {
  for (const auto& permissions_data : permissions_data_list) {
    if (IsUnusedPermissionRevocation(permissions_data.revocation_type)) {
      unused_site_permissions_manager_
          ->StorePermissionInUnusedSitePermissionSetting(
              permissions_data.permission_types,
              permissions_data.chooser_permissions_data,
              permissions_data.constraints.Clone(),
              permissions_data.primary_pattern,
              ContentSettingsPattern::Wildcard());
    }

    if (IsAbusiveNotificationAutoRevocationEnabled() &&
        IsAbusiveNotificationPermissionRevocation(
            permissions_data.revocation_type)) {
      abusive_notification_manager_->RestoreDeletedRevokedPermission(
          permissions_data.primary_pattern,
          permissions_data.constraints.Clone());
    }

    if (disruptive_notification_manager_ &&
        IsDisruptiveNotificationPermissionRevocation(
            permissions_data.revocation_type)) {
      disruptive_notification_manager_->RestoreDeletedRevokedPermission(
          permissions_data.primary_pattern,
          permissions_data.constraints.Clone());
    }
  }
}

void RevokedPermissionsService::OnPermissionsAutorevocationControlChanged() {
  // TODO(crbug.com/40250875): Clean up these checks.
  if (IsUnusedSiteAutoRevocationEnabled() ||
      IsAbusiveNotificationAutoRevocationEnabled()) {
    StartRepeatedUpdates();
  } else {
    StopTimer();
  }
}

std::vector<ContentSettingEntry>
RevokedPermissionsService::GetTrackedUnusedPermissionsForTesting() {
  return unused_site_permissions_manager_
      ->GetTrackedUnusedPermissionsForTesting();  // IN-TEST
}

void RevokedPermissionsService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
  if (disruptive_notification_manager_) {
    disruptive_notification_manager_->SetClockForTesting(clock);  // IN-TEST
  }
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->SetClockForTesting(clock);  // IN-TEST
  }
  unused_site_permissions_manager_->SetClockForTesting(clock);  // IN-TEST
}

base::WeakPtr<SafetyHubService> RevokedPermissionsService::GetAsWeakRef() {
  return weak_factory_.GetWeakPtr();
}

bool RevokedPermissionsService::IsUnusedSiteAutoRevocationEnabled() {
  return pref_change_registrar_->prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled);
}

bool RevokedPermissionsService::IsAbusiveNotificationAutoRevocationEnabled() {
  return safe_browsing::IsSafeBrowsingEnabled(*pref_change_registrar_->prefs());
}
