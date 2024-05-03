// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FALLBACK_URL_UTIL_H_
#define COMPONENTS_FAVICON_CORE_FALLBACK_URL_UTIL_H_

#include <string>


class GURL;

namespace favicon {

// Returns a very short string (e.g., capitalized first letter in a domain's
// name) to represent `url`.
std::u16string GetFallbackIconText(const GURL& url);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FALLBACK_URL_UTIL_H_
