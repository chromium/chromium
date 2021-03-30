// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_

#include "base/optional.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "url/origin.h"

class GURL;
class HostContentSettingsMap;

namespace content_settings {
class CookieSettings;
}

namespace embedder_support {

// See ContentBrowserClient::AllowAppCache.
bool AllowAppCache(const GURL& manifest_url,
                   const GURL& site_for_cookies,
                   const base::Optional<url::Origin>& top_frame_origin,
                   const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowServiceWorker.
content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const content_settings::CookieSettings* cookie_settings,
    const HostContentSettingsMap* settings_map);

// See ContentBrowserClient::AllowSharedWorker.
bool AllowSharedWorker(const GURL& worker_url,
                       const GURL& site_for_cookies,
                       const base::Optional<url::Origin>& top_frame_origin,
                       const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerFileSystem.
bool AllowWorkerFileSystem(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerIndexedDB.
bool AllowWorkerIndexedDB(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerCacheStorage.
bool AllowWorkerCacheStorage(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerWebLocks.
bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_
