// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/content_settings_utils.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "url/gurl.h"

namespace embedder_support {

bool AllowAppCache(const GURL& manifest_url,
                   const GURL& site_for_cookies,
                   const base::Optional<url::Origin>& top_frame_origin,
                   const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(manifest_url, site_for_cookies,
                                                top_frame_origin);
}

content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
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
  bool allow_cookies = cookie_settings->IsCookieAccessAllowed(
      scope, site_for_cookies, top_frame_origin);

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

bool AllowSharedWorker(
    const GURL& worker_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(worker_url, site_for_cookies,
                                                top_frame_origin);
}

bool AllowWorkerFileSystem(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);
}

bool AllowWorkerIndexedDB(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);
}

bool AllowWorkerCacheStorage(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);
}

bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  return cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);
}

}  // namespace embedder_support
