// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/content_settings_utils.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace embedder_support {

bool AllowAppCache(const GURL& manifest_url,
                   const GURL& site_for_cookies,
                   const absl::optional<url::Origin>& top_frame_origin,
                   const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsFullCookieAccessAllowed(
      manifest_url, site_for_cookies, top_frame_origin);
}

content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const content_settings::CookieSettings* cookie_settings,
    const HostContentSettingsMap* settings_map) {
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();
  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> value = settings_map->GetWebsiteSetting(
      first_party_url, first_party_url, ContentSettingsType::JAVASCRIPT, &info);
  ContentSetting setting = content_settings::ValueToContentSetting(value.get());
  bool allow_javascript = setting == CONTENT_SETTING_ALLOW;

  // Check if cookies are allowed.
  bool allow_cookies = cookie_settings->IsFullCookieAccessAllowed(
      scope, site_for_cookies, top_frame_origin);

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

bool AllowSharedWorker(
    const GURL& worker_url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    int render_process_id,
    int render_frame_id,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      worker_url, site_for_cookies, top_frame_origin);

  content_settings::PageSpecificContentSettings::SharedWorkerAccessed(
      render_process_id, render_frame_id, worker_url, name, storage_key,
      !allow);
  return allow;
}

bool AllowWorkerFileSystem(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      url, url, url::Origin::Create(url));
  for (const auto& it : render_frames) {
    content_settings::PageSpecificContentSettings::FileSystemAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }
  return allow;
}

bool AllowWorkerIndexedDB(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      url, url, url::Origin::Create(url));

  for (const auto& it : render_frames) {
    content_settings::PageSpecificContentSettings::IndexedDBAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }
  return allow;
}

bool AllowWorkerCacheStorage(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings) {
  bool allow = cookie_settings->IsFullCookieAccessAllowed(
      url, url, url::Origin::Create(url));

  for (const auto& it : render_frames) {
    content_settings::PageSpecificContentSettings::CacheStorageAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }
  return allow;
}

bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsFullCookieAccessAllowed(url, url,
                                                    url::Origin::Create(url));
}

}  // namespace embedder_support
