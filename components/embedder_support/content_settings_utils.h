// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_

#include <optional>

#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "url/origin.h"

class GURL;
class HostContentSettingsMap;

namespace blink {
class StorageKey;
}  // namespace blink

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace net {
class SiteForCookies;
}  // namespace net

namespace embedder_support {

// See ContentBrowserClient::AllowServiceWorker.
content::AllowServiceWorkerResult AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const content_settings::CookieSettings* cookie_settings,
    const HostContentSettingsMap* settings_map);

// See ContentBrowserClient::AllowSharedWorker. This also notifies content
// settings of shared worker access.
bool AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
    int render_process_id,
    int render_frame_id,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerFileSystem. This also notifies content
// settings of file system access.
bool AllowWorkerFileSystem(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerIndexedDB. This also notifies content
// settings of Indexed DB access.
bool AllowWorkerIndexedDB(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerCacheStorage. This also notifies content
// settings of cache storage access.
bool AllowWorkerCacheStorage(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    const content_settings::CookieSettings* cookie_settings);

// See ContentBrowserClient::AllowWorkerWebLocks.
bool AllowWorkerWebLocks(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_CONTENT_SETTINGS_UTILS_H_
