// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_util.h"

#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace translate {

bool IsTranslatableURL(const GURL& url) {
  // A URLs is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL (chrome:// and others)
  // - the devtools (which is considered UI)
  // - about:blank
  // - Chrome OS file manager extension [but not able to check here]
  // Note: this is duplicated logic from TranslateService (missing a chromeos
  // file manager check at this components level).
  // TODO(crbug.com/40123934) Reuse this utility in TranslateService once
  // subframe translation launched. Note that the chromeos checks in the
  // TranslateService version will still need to be at the browser level.
  return !url.is_empty() && !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) && !url.IsAboutBlank();
}

}  // namespace translate
