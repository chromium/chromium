// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_OFFLINE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_OFFLINE_H_

#include "content/public/browser/browser_context.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "url/gurl.h"

namespace web_app {
content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::BrowserContext* browser_context,
    int32_t error_code);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_OFFLINE_H_
