// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_WEB_UI_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_WEB_UI_CONSTANTS_H_

#include "url/gurl.h"

namespace safe_browsing {

extern const char kChromeUISafeBrowsingURL[];
extern const char kChromeUISafeBrowsingHost[];
extern const char kChromeUISafeBrowsingMatchBillingUrl[];
extern const char kChromeUISafeBrowsingMatchMalwareUrl[];
extern const char kChromeUISafeBrowsingMatchPhishingUrl[];
extern const char kChromeUISafeBrowsingMatchUnwantedUrl[];

bool IsSafeBrowsingWebUIUrl(const GURL& url);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_WEB_UI_CONSTANTS_H_
