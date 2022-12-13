// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/unused_site_permissions_service.h"

#include "base/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
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
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr char kRevokedKey[] = "revoked";
constexpr base::TimeDelta kRevocationThreshold = base::Days(60);

namespace permissions {
namespace {

// Called on a background thread.
UnusedSitePermissionsService::UnusedPermissionMap GetUnusedPermissionsMap(
    base::Clock* clock,
    scoped_refptr<HostContentSettingsMap> hcsm) {
  UnusedSitePermissionsService::UnusedPermissionMap recently_unused;
  base::Time threshold =
      clock->Now() - content_settings::GetCoarseVisitedTimePrecision();

  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    if (!content_settings::CanTrackLastVisit(type))
      continue;
    ContentSettingsForOneType settings;
    hcsm->GetSettingsForOneType(type, &settings);
    for (const auto& setting : settings) {
      // Skip wildcard patterns that don't belong to a single origin. These
      // shouldn't track visit timestamps.
      if (!setting.primary_pattern.MatchesSingleOrigin())
        continue;
      if (setting.metadata.last_visited != base::Time() &&
          setting.metadata.last_visited < threshold) {
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
  return recently_unused;
}

void StorePermissionInRevokedPermissionSetting(
    const std::list<UnusedSitePermissionsService::ContentSettingEntry>&
        recently_revoked_permissions,
    scoped_refptr<HostContentSettingsMap> hcsm) {
  DCHECK(!recently_revoked_permissions.empty());
  const ContentSettingsPattern& primary_pattern =
      recently_revoked_permissions.front().source.primary_pattern;
  const ContentSettingsPattern& secondary_pattern =
      recently_revoked_permissions.front().source.secondary_pattern;

  GURL url = GURL(primary_pattern.ToString());
  // The url should be valid as it is checked that the pattern represents a
  // single origin.
  DCHECK(url.is_valid());
  // Get the current value of the setting to append the recently revoked
  // permissions.
  base::Value cur_value(hcsm->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, nullptr));

  base::Value::Dict dict = cur_value.is_dict() ? std::move(cur_value.GetDict())
                                               : base::Value::Dict();
  base::Value::List permission_type_list =
      dict.FindList(kRevokedKey) ? std::move(*dict.FindList(kRevokedKey))
                                 : base::Value::List();

  for (const auto& permission : recently_revoked_permissions) {
    permission_type_list.Append(static_cast<int32_t>(permission.type));
  }

  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  // Set website setting for the list of recently revoked permissions and
  // previously revoked permissions, if exists.
  hcsm->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)));
}

}  // namespace

UnusedSitePermissionsService::TabHelper::TabHelper(
    content::WebContents* web_contents,
    UnusedSitePermissionsService* unused_site_permission_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents),
      unused_site_permission_service_(
          unused_site_permission_service->AsWeakPtr()) {}

UnusedSitePermissionsService::TabHelper::~TabHelper() = default;

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
}

UnusedSitePermissionsService::~UnusedSitePermissionsService() = default;

void UnusedSitePermissionsService::Shutdown() {
  update_timer_.Stop();
}

void UnusedSitePermissionsService::StartRepeatedUpdates() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateUnusedPermissionsAsync(base::NullCallback());
  base::TimeDelta repeated_update_interval =
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval.Get();
  update_timer_.Start(
      FROM_HERE, repeated_update_interval,
      base::BindRepeating(
          &UnusedSitePermissionsService::UpdateUnusedPermissionsAsync,
          base::Unretained(this), base::NullCallback()));
}

void UnusedSitePermissionsService::UpdateUnusedPermissionsAsync(
    base::RepeatingClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetUnusedPermissionsMap, clock_, hcsm_),
      base::BindOnce(
          &UnusedSitePermissionsService::OnUnusedPermissionsMapRetrieved,
          AsWeakPtr(), std::move(callback)));
}

// Called by TabHelper when a URL was visited.
void UnusedSitePermissionsService::OnPageVisited(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if this origin has unused permissions.
  auto origin_entry = recently_unused_permissions_.find(origin.Serialize());
  if (origin_entry == recently_unused_permissions_.end())
    return;

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

void UnusedSitePermissionsService::OnUnusedPermissionsMapRetrieved(
    base::OnceClosure callback,
    UnusedPermissionMap map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  recently_unused_permissions_ = map;
  RevokeUnusedPermissions();
  if (callback)
    std::move(callback).Run();
}

void UnusedSitePermissionsService::RevokeUnusedPermissions() {
  base::Time threshold = clock_->Now() - kRevocationThreshold;

  for (auto itr = recently_unused_permissions_.begin();
       itr != recently_unused_permissions_.end();) {
    std::list<ContentSettingEntry>& unused_site_permissions = itr->second;

    std::list<ContentSettingEntry> revoked_permissions;
    for (auto permission_itr = unused_site_permissions.begin();
         permission_itr != unused_site_permissions.end();) {
      const ContentSettingEntry& entry = *permission_itr;
      // Reset the permission to default if the site is visited before
      // threshold.
      DCHECK(entry.source.metadata.last_visited != base::Time());
      if (entry.source.metadata.last_visited < threshold) {
        revoked_permissions.push_back(entry);
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
      StorePermissionInRevokedPermissionSetting(revoked_permissions, hcsm_);
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

void UnusedSitePermissionsService::UpdateUnusedPermissionsForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop loop;
  UpdateUnusedPermissionsAsync(loop.QuitClosure());
  loop.Run();
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

}  // namespace permissions
