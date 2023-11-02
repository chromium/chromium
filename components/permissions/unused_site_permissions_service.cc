// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/unused_site_permissions_service.h"
#include "base/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace permissions {
namespace {

// Called on a background thread.
UnusedSitePermissionsService::UnusedPermissionMap GetUnusedPermissionsMap(
    base::Clock* clock,
    scoped_refptr<HostContentSettingsMap> hcsm) {
  UnusedSitePermissionsService::UnusedPermissionMap recently_unused;
  base::Time threshold =
      clock->Now() - content_settings::GetCoarseTimePrecision();

  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    if (!content_settings::CanTrackLastVisit(type))
      continue;
    ContentSettingsForOneType settings;
    hcsm->GetSettingsForOneType(type, &settings);
    for (const auto& setting : settings) {
      // Skip wildcard pattern that are not host specific. These shouldn't
      // track visit timestamps as they would match any URL.
      if (setting.primary_pattern.GetHost().empty())
        continue;
      if (setting.metadata.last_visited != base::Time() &&
          setting.metadata.last_visited < threshold) {
        auto& list = recently_unused[setting.primary_pattern.GetHost()];
        list.push_back({type, std::move(setting)});
      }
    }
  }
  return recently_unused;
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
  update_timer_.Start(
      FROM_HERE, base::Days(1),
      base::BindRepeating(
          &UnusedSitePermissionsService::UpdateUnusedPermissionsAsync,
          base::Unretained(this), base::NullCallback()));
}

void UnusedSitePermissionsService::UpdateUnusedPermissionsForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop loop;
  UpdateUnusedPermissionsAsync(loop.QuitClosure());
  loop.Run();
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
  // Check if this host has unused permissions.
  auto host_entry = recently_unused_permissions_.find(origin.host());
  if (host_entry == recently_unused_permissions_.end())
    return;

  // See which permissions of the host actually match the URL and update them.
  auto& site_permissions = host_entry->second;
  for (auto it = site_permissions.begin(); it != site_permissions.end();) {
    if (it->source.primary_pattern.Matches(origin.GetURL())) {
      hcsm_->UpdateLastVisitedTime(it->source.primary_pattern,
                                   it->source.secondary_pattern, it->type);
      site_permissions.erase(it++);
    } else {
      it++;
    }
  }
  // Remove host entry if all permissions were updated.
  if (site_permissions.empty()) {
    recently_unused_permissions_.erase(host_entry);
  }
}

void UnusedSitePermissionsService::OnUnusedPermissionsMapRetrieved(
    base::OnceClosure callback,
    UnusedPermissionMap map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  recently_unused_permissions_ = map;
  if (callback)
    std::move(callback).Run();
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
