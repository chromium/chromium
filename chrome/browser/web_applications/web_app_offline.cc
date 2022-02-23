// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_app_offline.h"

#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/error_page/common/error.h"

namespace web_app {
content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::BrowserContext* browser_context,
    int32_t error_code) {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsDefaultOfflinePage)) {
    return nullptr;
  }

  if (error_code != net::ERR_INTERNET_DISCONNECTED) {
    return nullptr;
  }

  return web_app::GetAppManifestInfo(url, browser_context);
#endif  //  BUILDFLAG(IS_ANDROID)
}

}  // namespace web_app
