// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/content_settings_utils.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
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
    const content_settings::CookieSettings* cookie_settings,
    const blink::StorageKey& storage_key) {
  // TODO(crbug.com/40247160): Consider whether the following check should
  // somehow determine real CookieSettingOverrides rather than default to none.
  std::optional<url::Origin> top_frame_origin =
      storage_key.IsFirstPartyContext()
          ? storage_key.origin()
          : url::Origin::Create(storage_key.top_level_site().GetURL());

  bool allow = cookie_settings->IsAnyStorageAccessAllowed(
      url, storage_key.ToNetSiteForCookies(), top_frame_origin,
      net::CookieSettingOverrides(), storage_key.ToCookiePartitionKey());

  for (const auto& it : render_frames) {
    auto* rfh = content::RenderFrameHost::FromID(it);
    if (!rfh) {
      continue;
    }
    content_settings::PageSpecificContentSettings::StorageAccessed(
        storage_type, it, rfh->GetStorageKey(), !allow);
  }

  return allow;
}
}  // namespace

content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const blink::StorageKey& storage_key,
    const content_settings::CookieSettings* cookie_settings,
    const HostContentSettingsMap* settings_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40847840): Remove this check once we figure out what is
  // wrong.
  DCHECK(settings_map);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();
  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  ContentSetting setting = settings_map->GetContentSetting(
      first_party_url, first_party_url, ContentSettingsType::JAVASCRIPT, &info);
  bool allow_javascript = setting == CONTENT_SETTING_ALLOW;

  // Check if cookies/storage are allowed.
  // TODO(crbug.com/40247160): Consider whether the following check should
  // also consider the third-party cookie user bypass override.
  bool allow_cookies = cookie_settings->IsAnyStorageAccessAllowed(
      scope, site_for_cookies, top_frame_origin, net::CookieSettingOverrides(),
      storage_key.ToCookiePartitionKey());

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

bool AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
    int render_process_id,
    int render_frame_id,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsAnyStorageAccessAllowed(
      worker_url, site_for_cookies, top_frame_origin,
      net::CookieSettingOverrides(), storage_key.ToCookiePartitionKey());

  content_settings::PageSpecificContentSettings::SharedWorkerAccessed(
      render_process_id, render_frame_id, worker_url, name, storage_key,
      same_site_cookies, !allow);
  return allow;
}

bool AllowWorkerFileSystem(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings,
    const blink::StorageKey& storage_key) {
  return AllowWorkerStorageAccess(StorageType::FILE_SYSTEM, url, render_frames,
                                  cookie_settings, storage_key);
}

bool AllowWorkerIndexedDB(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings,
    const blink::StorageKey& storage_key) {
  return AllowWorkerStorageAccess(StorageType::INDEXED_DB, url, render_frames,
                                  cookie_settings, storage_key);
}

bool AllowWorkerCacheStorage(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings,
    const blink::StorageKey& storage_key) {
  return AllowWorkerStorageAccess(StorageType::CACHE, url, render_frames,
                                  cookie_settings, storage_key);
}

bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings,
    const blink::StorageKey& storage_key) {
  return AllowWorkerStorageAccess(StorageType::WEB_LOCKS, url, {},
                                  cookie_settings, storage_key);
}

}  // namespace embedder_support
