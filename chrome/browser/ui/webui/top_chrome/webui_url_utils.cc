// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_url_utils.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

bool IsTopChromeWebUIURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.DomainIs(chrome::kChromeUITopChromeDomain);
}
