// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_ERROR_PAGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_ERROR_PAGE_H_

#include "content/public/common/alternative_error_page_override_info.mojom.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace web_app {

content::mojom::AlternativeErrorPageOverrideInfoPtr
MaybeGetIsolatedWebAppErrorPageInfo(const GURL& url,
                                    content::RenderFrameHost* render_frame_host,
                                    content::BrowserContext* browser_context,
                                    int32_t error_code);

}

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_ERROR_PAGE_H_
