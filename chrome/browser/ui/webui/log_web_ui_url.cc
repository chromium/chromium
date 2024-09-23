// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/log_web_ui_url.h"

#include <stdint.h>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace webui {

const char kWebUICreatedForUrl[] = "WebUI.CreatedForUrl";

bool LogWebUIUrl(const GURL& web_ui_url) {
  bool should_log = web_ui_url.SchemeIs(content::kChromeUIScheme) ||
                    web_ui_url.SchemeIs(content::kChromeUIUntrustedScheme) ||
                    web_ui_url.SchemeIs(content::kChromeDevToolsScheme);

  if (should_log) {
    uint32_t hash = base::Hash(web_ui_url.DeprecatedGetOriginAsURL().spec());
    base::UmaHistogramSparse(kWebUICreatedForUrl,
                             static_cast<base::HistogramBase::Sample>(hash));
  }

  return should_log;
}

}  // namespace webui
