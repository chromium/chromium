// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/content_settings_utils.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace embedder_support {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

namespace {
bool AllowWorkerStorageAccess(
    StorageType storage_type,
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  // TODO(crbug.com/1386190): Consider whether the following check should
  // somehow determine real CookieSettingOverrides rather than default to none.
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
      net::CookieSettingOverrides());
  // Allow storage when --test-third-party-cookie-phaseout is used, but ensure
  // that only partitioned storage is available. This developer flag is meant to
  // simulate Chrome's behavior when 3P cookies are turned down to help
  // developers test their site.
  if (!allow && net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    allow = true;
  }

  for (const auto& it : render_frames) {
    content_settings::PageSpecificContentSettings::StorageAccessed(
        storage_type, it.child_id, it.frame_routing_id, url, !allow);
  }

  return allow;
}
}  // namespace

content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const content_settings::CookieSettings* cookie_settings,
    const HostContentSettingsMap* settings_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/1336617): Remove this check once we figure out what is
  // wrong.
  DCHECK(settings_map);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();
  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  ContentSetting setting = settings_map->GetContentSetting(
      first_party_url, first_party_url, ContentSettingsType::JAVASCRIPT, &info);
  bool allow_javascript = setting == CONTENT_SETTING_ALLOW;

  // Check if cookies are allowed. Storage Access API grants and Top-Level
  // Storage Access API grants may only be considered if storage is partitioned
  // (or if Storage Access API is intended to grant access to storage - which is
  // a deviation from the spec, but at least one embedder wants that ability).
  // TODO(crbug.com/1386190): Consider whether the following check should
  // also consider the third-party cookie user bypass override.
  bool allow_cookies = cookie_settings->IsFullCookieAccessAllowed(
      scope, site_for_cookies, top_frame_origin,
      cookie_settings->SettingOverridesForStorage());
  // Allow storage when --test-third-party-cookie-phaseout is used, but ensure
  // that only partitioned storage is available. This developer flag is meant to
  // simulate Chrome's behavior when 3P cookies are turned down to help
  // developers test their site.
  if (!allow_cookies &&
      net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    allow_cookies = true;
  }

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

bool AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    int render_process_id,
    int render_frame_id,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      worker_url, site_for_cookies, top_frame_origin,
      cookie_settings->SettingOverridesForStorage());

  // Allow storage when --test-third-party-cookie-phaseout is used, but ensure
  // that only partitioned storage is available. This developer flag is meant to
  // simulate Chrome's behavior when 3P cookies are turned down to help
  // developers test their site.
  if (!allow && net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    allow = true;
  }

  content_settings::PageSpecificContentSettings::SharedWorkerAccessed(
      render_process_id, render_frame_id, worker_url, name, storage_key,
      !allow);
  return allow;
}

bool AllowWorkerFileSystem(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  return AllowWorkerStorageAccess(StorageType::FILE_SYSTEM, url, render_frames,
                                  cookie_settings);
}

bool AllowWorkerIndexedDB(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  return AllowWorkerStorageAccess(StorageType::INDEXED_DB, url, render_frames,
                                  cookie_settings);
}

bool AllowWorkerCacheStorage(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  return AllowWorkerStorageAccess(StorageType::CACHE, url, render_frames,
                                  cookie_settings);
}

bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return AllowWorkerStorageAccess(StorageType::WEB_LOCKS, url, {},
                                  cookie_settings);
}

}  // namespace embedder_support
