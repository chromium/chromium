// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_utils.h"

#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

bool IsTopChromeWebUIURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.DomainIs(chrome::kChromeUITopChromeDomain);
}

bool IsTopChromeUntrustedWebUIURL(const GURL& url) {
  // TODO(b/339037968): Remove this exception once Lens uses ".top-chrome"
  // suffix.
#if !BUILDFLAG(IS_ANDROID)
  if (url == GURL(chrome::kChromeUILensOverlayUntrustedURL)) {
    return true;
  }
#endif

  return url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         url.DomainIs(chrome::kChromeUITopChromeDomain);
}

bool ShouldRefuseBecomingKeyViewForTopChromeWebUI(const GURL& url) {
  // List of specific Top Chrome hosts that should not accept key view status.
  if (IsTopChromeWebUIURL(url) &&
      url.host() == chrome::kChromeUIWebUIToolbarHost) {
    return true;
  }

  return false;
}
