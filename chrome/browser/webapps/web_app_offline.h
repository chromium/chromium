// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_WEB_APP_OFFLINE_H_
#define CHROME_BROWSER_WEBAPPS_WEB_APP_OFFLINE_H_

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace web_app {

// Gets information from web app's manifest, including theme color, background
// color and app short name, and returns this inside a struct.
content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context);

// Tracks how long the offline page is showing and the reason why it gets
// closed (e.g. page is refreshed after coming back online or web contents gets
// destroyed).
void TrackOfflinePageVisibility(content::RenderFrameHost* render_frame_host);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEBAPPS_WEB_APP_OFFLINE_H_
