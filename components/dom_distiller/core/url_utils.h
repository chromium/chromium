// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_

#include <string>

#include "base/strings/string_piece_forward.h"

class GURL;

namespace dom_distiller {

namespace url_utils {

// Returns the URL for viewing distilled content for an entry.
const GURL GetDistillerViewUrlFromEntryId(const std::string& scheme,
                                          const std::string& entry_id);

// Returns the URL for viewing distilled content for a URL.
const GURL GetDistillerViewUrlFromUrl(const std::string& scheme,
                                      const GURL& view_url,
                                      int64_t start_time_ms = 0);

// Returns the original URL from the distilled URL.
// If |distilled_url| is not distilled, it is returned as is.
// If |distilled_url| looks like distilled, but no original URL can be found,
// an empty, invalid URL is returned.
const GURL GetOriginalUrlFromDistillerUrl(const GURL& distilled_url);

// Returns the starting time from the distilled URL.
// Returns 0 when not available or on error.
int64_t GetTimeFromDistillerUrl(const GURL& url);

// Returns the value of the query parameter for the given |key| for a given URL.
// If the URL is invalid or if the key is not found, returns an empty string.
// If there are multiple keys found in the URL, returns the value for the first
// key.
std::string GetValueForKeyInUrl(const GURL& url, const std::string& key);

// Returns the value of the query parameter for the given path.
std::string GetValueForKeyInUrlPathQuery(const std::string& path,
                                         const std::string& key);

// Returns whether it should be possible to distill the given |url|.
bool IsUrlDistillable(const GURL& url);

// Returns whether the given |url| is for a distilled page.
bool IsDistilledPage(const GURL& url);

}  // namespace url_utils

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
