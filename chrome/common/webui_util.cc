// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_util.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace chrome {

bool ShouldUseCodeCacheForWebUIUrl(const GURL& request_url) {
  DCHECK(content::HasWebUIScheme(request_url));
#if !BUILDFLAG(IS_ANDROID)
  if (features::kRestrictedWebUICodeCache.Get()) {
    if (request_url.host() == chrome::kChromeUITabSearchHost) {
      return true;
    }
    return false;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return true;
}

}  // namespace chrome
