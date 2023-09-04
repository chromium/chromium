// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr base::TimeDelta kRevocationThresholdNoDelayForTesting = base::Days(0);
constexpr base::TimeDelta kRevocationThresholdWithDelayForTesting =
    base::Minutes(5);
constexpr base::TimeDelta kRevocationCleanUpThresholdWithDelayForTesting =
    base::Minutes(30);

namespace {
// Reflects the maximum number of days between a permissions being revoked and
// the time when the user regrants the permission through the unused site
// permission module of Safete Check. The maximum number of days is determined
// by `kRevocationCleanUpThreshold`.
size_t kAllowAgainMetricsExclusiveMaxCount = 31;

// Using a single bucket per day, following the value of
// |kAllowAgainMetricsExclusiveMaxCount|.
size_t kAllowAgainMetricsBuckets = 31;

base::TimeDelta GetRevocationThreshold() {
  // TODO(crbug.com/1401701): Clean up no delay revocation after the feature is
  // ready. Today, no delay revocation is necessary to enable manual testing.
  if (content_settings::features::kSafetyCheckUnusedSitePermissionsNoDelay
          .Get()) {
    return kRevocationThresholdNoDelayForTesting;
  } else if (content_settings::features::
                 kSafetyCheckUnusedSitePermissionsWithDelay.Get()) {
    return kRevocationThresholdWithDelayForTesting;
  }
  return content_settings::features::
      kSafetyCheckUnusedSitePermissionsRevocationThreshold.Get();
}

base::TimeDelta GetCleanUpThreshold() {
  // TODO(crbug.com/1401701): Clean up delayed clean up logic after the feature
  // is ready. Today, this is necessary to enable manual testing.
  if (content_settings::features::kSafetyCheckUnusedSitePermissionsWithDelay
          .Get()) {
    return kRevocationCleanUpThresholdWithDelayForTesting;
  }
  return content_settings::features::
      kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get();
}

}  // namespace

base::TimeDelta UnusedSitePermissionsService::GetRepeatedUpdateInterval() {
  return content_settings::features::
      kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval.Get();
}

UnusedSitePermissionsService::TabHelper::TabHelper(
    content::WebContents* web_contents,
    UnusedSitePermissionsService* unused_site_permission_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents),
      unused_site_permission_service_(
          base::AsWeakPtr(unused_site_permission_service)) {}

UnusedSitePermissionsService::TabHelper::~TabHelper() = default;

UnusedSitePermissionsService::RevokedPermission::RevokedPermission(
    ContentSettingsPattern origin,
    std::set<ContentSettingsType> permission_types,
    base::Time expiration)
    : origin(origin),
      permission_types(permission_types),
      expiration(expiration) {}

UnusedSitePermissionsService::RevokedPermission::~RevokedPermission() = default;

UnusedSitePermissionsService::RevokedPermission::RevokedPermission(
    const RevokedPermission&) = default;

UnusedSitePermissionsService::UnusedSitePermissionsResult::
    UnusedSitePermissionsResult() = default;
UnusedSitePermissionsService::UnusedSitePermissionsResult::
    ~UnusedSitePermissionsResult() = default;

void UnusedSitePermissionsService::UnusedSitePermissionsResult::
    AddRevokedPermission(ContentSettingsPattern origin,
                         std::set<ContentSettingsType> permission_types,
                         base::Time expiration) {
  RevokedPermission revoked_permission(std::move(origin), permission_types,
                                       std::move(expiration));
  revoked_permissions_.push_back(std::move(revoked_permission));
}

std::list<UnusedSitePermissionsService::RevokedPermission>
UnusedSitePermissionsService::UnusedSitePermissionsResult::
    GetRevokedPermissions() {
  std::list<UnusedSitePermissionsService::RevokedPermission> result(
      revoked_permissions_);
  return result;
}

void UnusedSitePermissionsService::TabHelper::PrimaryPageChanged(
    content::Page& page) {
  if (unused_site_permission_service_) {
    unused_site_permission_service_->OnPageVisited(
        page.GetMainDocument().GetLastCommittedOrigin());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UnusedSitePermissionsService::TabHelper);

UnusedSitePermissionsService::UnusedSitePermissionsService(
    HostContentSettingsMap* hcsm)
    : hcsm_(hcsm), clock_(base::DefaultClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content_settings_observation_.Observe(hcsm);
  StartRepeatedUpdates();
  InitializeLatestResult();
}

UnusedSitePermissionsService::~UnusedSitePermissionsService() = default;

void UnusedSitePermissionsService::InitializeLatestResult() {
  latest_result_ = GetRevokedPermissions();
}

void UnusedSitePermissionsService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(
          ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)) {
    return;
  }

  // When permissions change for a pattern it is either (1) through resetting
  // permissions, e.g. in page info or site settings, (2) user modifying
  // permissions manually, or (3) through the auto-revocation that this module
  // performs. In (1) and (2) the pattern should no longer be shown to the user.
  // 1: After resetting permissions the browser state should be in a
  //    state as if the permission had never been granted.
  // 2: The user is actively engaging with the permissions of the site, so it is
  //    no longer considered an unused site for the purposes of this module.
  //    This includes the case where unrelated permissions to the revoked ones
  //    are changed.
  // 3: Current logic ensures this does not happen for sites that already have
  //    revoked permissions. This module revokes permissions in an all-or-none
  //    fashion.
  DeletePatternFromRevokedPermissionList(primary_pattern, secondary_pattern);
}

void UnusedSitePermissionsService::Shutdown() {
  update_timer_.Stop();
  content_settings_observation_.Reset();
}

void UnusedSitePermissionsService::RegrantPermissionsForOrigin(
    const url::Origin& origin) {
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm_->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &info));

  if (!stored_value.is_dict()) {
    return;
  }

  base::Value::List* permission_type_list =
      stored_value.GetDict().FindList(permissions::kRevokedKey);
  CHECK(permission_type_list);

  // Re-grant the permissions that are revoked. This service only auto-revokes
  // permissions that were ALLOW, so re-granting will switch those permissions
  // back to ALLOW again.
  for (auto& permission_type : *permission_type_list) {
    // Check if stored permission type is valid.
    auto type_int = permission_type.GetIfInt();
    if (!type_int.has_value()) {
      continue;
    }
    hcsm_->SetContentSettingCustomScope(
        info.primary_pattern, info.secondary_pattern,
        static_cast<ContentSettingsType>(type_int.value()),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  // Ignore origin from future auto-revocations.
  IgnoreOriginForAutoRevocation(origin);

  // Remove origin from revoked permissions list.
  DeletePatternFromRevokedPermissionList(info.primary_pattern,
                                         info.secondary_pattern);

  // Record the days elapsed from auto-revocation to regrant.
  base::Time revoked_time =
      info.metadata.expiration() -
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get();
  base::UmaHistogramCustomCounts(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays",
      (clock_->Now() - revoked_time).InDays(), 0,
      kAllowAgainMetricsExclusiveMaxCount, kAllowAgainMetricsBuckets);
}

void UnusedSitePermissionsService::UndoRegrantPermissionsForOrigin(
    const std::set<ContentSettingsType> permissions,
    const absl::optional<content_settings::ContentSettingConstraints>
        constraint,
    const url::Origin origin) {
  for (const auto& permission : permissions) {
    hcsm_->SetContentSettingCustomScope(
        ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
        ContentSettingsPattern::Wildcard(), permission,
        ContentSetting::CONTENT_SETTING_DEFAULT);
  }

  StorePermissionInRevokedPermissionSetting(permissions, constraint, origin);
}

void UnusedSitePermissionsService::ClearRevokedPermissionsList() {
  for (const auto& revoked_permissions : hcsm_->GetSettingsForOneType(
           ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)) {
    DeletePatternFromRevokedPermissionList(
        revoked_permissions.primary_pattern,
        revoked_permissions.secondary_pattern);
  }
}

void UnusedSitePermissionsService::IgnoreOriginForAutoRevocation(
    const url::Origin& origin) {
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();

  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    for (const auto& setting : hcsm_->GetSettingsForOneType(type)) {
      if (setting.metadata.last_visited() != base::Time() &&
          setting.primary_pattern.MatchesSingleOrigin() &&
          setting.primary_pattern.Matches(origin.GetURL())) {
        hcsm_->ResetLastVisitedTime(setting.primary_pattern,
                                    setting.secondary_pattern, type);
        break;
      }
    }
  }
}

// Called by TabHelper when a URL was visited.
void UnusedSitePermissionsService::OnPageVisited(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if this origin has unused permissions.
  auto origin_entry = recently_unused_permissions_.find(origin.Serialize());
  if (origin_entry == recently_unused_permissions_.end()) {
    return;
  }

  // See which permissions of the origin actually match the URL and update them.
  auto& site_permissions = origin_entry->second;
  for (auto it = site_permissions.begin(); it != site_permissions.end();) {
    if (it->source.primary_pattern.Matches(origin.GetURL())) {
      hcsm_->UpdateLastVisitedTime(it->source.primary_pattern,
                                   it->source.secondary_pattern, it->type);
      site_permissions.erase(it++);
    } else {
      it++;
    }
  }
  // Remove origin entry if all permissions were updated.
  if (site_permissions.empty()) {
    recently_unused_permissions_.erase(origin_entry);
  }
}

void UnusedSitePermissionsService::DeletePatternFromRevokedPermissionList(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, {});
}

base::OnceCallback<std::unique_ptr<SafetyHubService::Result>()>
UnusedSitePermissionsService::GetBackgroundTask() {
  return base::BindOnce(&UnusedSitePermissionsService::UpdateOnBackgroundThread,
                        clock_, hcsm_);
}

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::UpdateOnBackgroundThread(
    base::Clock* clock,
    const scoped_refptr<HostContentSettingsMap> hcsm) {
  UnusedSitePermissionsService::UnusedPermissionMap recently_unused;
  base::Time threshold =
      clock->Now() - content_settings::GetCoarseVisitedTimePrecision();
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    if (!content_settings::CanTrackLastVisit(type)) {
      continue;
    }
    ContentSettingsForOneType settings = hcsm->GetSettingsForOneType(type);
    for (const auto& setting : settings) {
      // Skip wildcard patterns that don't belong to a single origin. These
      // shouldn't track visit timestamps.
      if (!setting.primary_pattern.MatchesSingleOrigin()) {
        continue;
      }
      if (setting.metadata.last_visited() != base::Time() &&
          setting.metadata.last_visited() < threshold) {
        GURL url = GURL(setting.primary_pattern.ToString());
        // Converting URL to a origin is normally an anti-pattern but here it is
        // ok since the URL belongs to a single origin. Therefore, it has a
        // fully defined URL+scheme+port which makes converting URL to origin
        // successful.
        url::Origin origin = url::Origin::Create(url);
        recently_unused[origin.Serialize()].push_back(
            {type, std::move(setting)});
      }
    }
  }

  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  result->SetRecentlyUnusedPermissions(recently_unused);
  return std::move(result);
}

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::UpdateOnUIThread(
    std::unique_ptr<SafetyHubService::Result> result) {
  auto* interim_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          result.get());
  recently_unused_permissions_ = interim_result->GetRecentlyUnusedPermissions();
  RevokeUnusedPermissions();
  return GetRevokedPermissions();
}

std::unique_ptr<UnusedSitePermissionsService::Result>
UnusedSitePermissionsService::GetRevokedPermissions() {
  ContentSettingsForOneType settings = hcsm_->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();

  for (const auto& revoked_permissions : settings) {
    ContentSettingsPattern origin = revoked_permissions.primary_pattern;
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());

    const base::Value::List* type_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    CHECK(type_list);
    std::set<ContentSettingsType> permission_types;
    for (base::Value& type : type_list->Clone()) {
      permission_types.insert(static_cast<ContentSettingsType>(type.GetInt()));
    }

    base::Time expiration = revoked_permissions.metadata.expiration();

    result->AddRevokedPermission(origin, permission_types, expiration);
  }
  return result;
}

void UnusedSitePermissionsService::RevokeUnusedPermissions() {
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kSafetyCheckUnusedSitePermissions)) {
    return;
  }

  base::Time threshold = clock_->Now() - GetRevocationThreshold();

  for (auto itr = recently_unused_permissions_.begin();
       itr != recently_unused_permissions_.end();) {
    std::list<ContentSettingEntry>& unused_site_permissions = itr->second;

    // All |primary_pattern|s are equal across list items, the same is true for
    // |secondary_pattern|s. This property is needed later and checked in the
    // loop.
    ContentSettingsPattern primary_pattern =
        unused_site_permissions.front().source.primary_pattern;
    ContentSettingsPattern secondary_pattern =
        unused_site_permissions.front().source.secondary_pattern;

    std::set<ContentSettingsType> revoked_permissions;
    for (auto permission_itr = unused_site_permissions.begin();
         permission_itr != unused_site_permissions.end();) {
      const ContentSettingEntry& entry = *permission_itr;
      // Check if the current permission can be auto revoked.
      ContentSetting setting =
          content_settings::ValueToContentSetting(entry.source.setting_value);
      if (!content_settings::CanBeAutoRevoked(entry.type, setting)) {
        permission_itr++;
        continue;
      }

      DCHECK_EQ(entry.source.primary_pattern, primary_pattern);
      DCHECK(entry.source.secondary_pattern ==
                 ContentSettingsPattern::Wildcard() ||
             entry.source.secondary_pattern == entry.source.primary_pattern);

      // Reset the permission to default if the site is visited before
      // threshold. Also, the secondary pattern should be wildcard.
      DCHECK_NE(entry.source.metadata.last_visited(), base::Time());
      DCHECK(entry.type != ContentSettingsType::NOTIFICATIONS);
      if (entry.source.metadata.last_visited() < threshold &&
          entry.source.secondary_pattern ==
              ContentSettingsPattern::Wildcard()) {
        revoked_permissions.insert(entry.type);
        hcsm_->SetContentSettingCustomScope(
            entry.source.primary_pattern, entry.source.secondary_pattern,
            entry.type, ContentSetting::CONTENT_SETTING_DEFAULT);
        unused_site_permissions.erase(permission_itr++);
      } else {
        permission_itr++;
      }
    }

    // Store revoked permissions on HCSM.
    if (!revoked_permissions.empty()) {
      StorePermissionInRevokedPermissionSetting(revoked_permissions,
                                                absl::nullopt, primary_pattern,
                                                secondary_pattern);
    }

    // Handle clean up of recently_unused_permissions_ map after revocation.
    if (unused_site_permissions.empty()) {
      // Since all unused permissions are revoked, the map should be cleared.
      recently_unused_permissions_.erase(itr++);
    } else {
      // Since there are some permissions that are not revoked, the tracked
      // unused permissions should be set to those permissions.
      // Note that, currently all permissions belong to a single domain will
      // revoked all together, since triggering permission prompt requires a
      // page visit. So the timestamp of all granted permissions of the origin
      // will be updated. However, this logic will prevent edge cases like
      // permission prompt stays open long time, also will provide support for
      // revoking permissions separately in the future.
      itr++;
    }
  }
}

void UnusedSitePermissionsService::StorePermissionInRevokedPermissionSetting(
    const std::set<ContentSettingsType> permissions,
    const absl::optional<content_settings::ContentSettingConstraints>
        constraint,
    const url::Origin origin) {
  // The |secondary_pattern| for
  // |ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS| is always wildcard.
  StorePermissionInRevokedPermissionSetting(
      permissions, constraint,
      ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
      ContentSettingsPattern::Wildcard());
}

void UnusedSitePermissionsService::StorePermissionInRevokedPermissionSetting(
    const std::set<ContentSettingsType> permissions,
    const absl::optional<content_settings::ContentSettingConstraints>
        constraint,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  GURL url = GURL(primary_pattern.ToString());
  // The url should be valid as it is checked that the pattern represents a
  // single origin.
  DCHECK(url.is_valid());
  // Get the current value of the setting to append the recently revoked
  // permissions.
  base::Value cur_value(hcsm_->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS));

  base::Value::Dict dict = cur_value.is_dict() ? std::move(cur_value.GetDict())
                                               : base::Value::Dict();
  base::Value::List permission_type_list =
      dict.FindList(permissions::kRevokedKey)
          ? std::move(*dict.FindList(permissions::kRevokedKey))
          : base::Value::List();

  for (const auto& permission : permissions) {
    permission_type_list.Append(static_cast<int32_t>(permission));
  }

  dict.Set(permissions::kRevokedKey,
           base::Value::List(std::move(permission_type_list)));

  content_settings::ContentSettingConstraints default_constraint(clock_->Now());
  default_constraint.set_lifetime(GetCleanUpThreshold());

  // Set website setting for the list of recently revoked permissions and
  // previously revoked permissions, if exists.
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)),
      constraint.has_value() ? constraint.value() : default_constraint);
}

std::vector<UnusedSitePermissionsService::ContentSettingEntry>
UnusedSitePermissionsService::GetTrackedUnusedPermissionsForTesting() {
  std::vector<ContentSettingEntry> result;
  for (const auto& list : recently_unused_permissions_) {
    for (const auto& entry : list.second) {
      result.push_back(entry);
    }
  }
  return result;
}

void UnusedSitePermissionsService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

base::WeakPtr<SafetyHubService> UnusedSitePermissionsService::GetAsWeakRef() {
  return weak_factory_.GetWeakPtr();
}
