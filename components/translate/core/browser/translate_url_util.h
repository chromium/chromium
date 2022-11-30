// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_UTIL_H_

#include "url/gurl.h"

namespace translate {

// Appends Translate API Key as a part of query to a passed |url|, and returns
// GURL instance.
GURL AddApiKeyToUrl(const GURL& url);

// Appends host locale parameter as a part of query to a passed |url|, and
// returns GURL instance.
GURL AddHostLocaleToUrl(const GURL& url);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_UTIL_H_
