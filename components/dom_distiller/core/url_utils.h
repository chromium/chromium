// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_

#include <string>

class GURL;

namespace dom_distiller {

namespace url_utils {

// Returns the URL for viewing distilled content for an entry.
// This is only used for testing.
const GURL GetDistillerViewUrlFromEntryId(const std::string& scheme,
                                          const std::string& entry_id);

// Returns the URL for viewing distilled content for |view_url|. This URL should
// not be displayed to end users (except in DevTools and view-source). Instead,
// users should always be shown the original page URL minus the http or https
// scheme in the omnibox (i.e. in LocationBarModel::GetFormattedURL()).
// A distilled page's true URL, the distiller view URL, should be returned
// from WebContents::GetLastCommittedURL() and WebContents::GetVisibleURL().
// This has the chrome-distiller scheme and the form
// chrome-distiller://<hash>?<params>, where <params> are generated from
// |view_url| and |start_time_ms|.
const GURL GetDistillerViewUrlFromUrl(const std::string& scheme,
                                      const GURL& view_url,
                                      const std::string& title,
                                      int64_t start_time_ms = 0);

// Returns the original article's URL from the distilled URL.
// If |distilled_url| is not distilled, it is returned as is.
// If |distilled_url| looks like distilled, but no original URL can be found,
// an empty, invalid URL is returned.
GURL GetOriginalUrlFromDistillerUrl(const GURL& distilled_url);

// Returns the starting time from the distilled URL.
// Returns 0 when not available or on error.
int64_t GetTimeFromDistillerUrl(const GURL& url);

// Returns the title of the original page from the distilled URL. Returns an
// empty string if not available or on error.
std::string GetTitleFromDistillerUrl(const GURL& url);

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

// Returns whether the given |url| is for a distilled page. This means the
// format of the URL is proper for a distilled page and that it encodes a
// valid article URL.
bool IsDistilledPage(const GURL& url);

// Returns whether the given |url| is formatted as if it were for a distilled
// page, i.e. it is valid and has a chrome-distiller:// scheme.
bool IsUrlDistilledFormat(const GURL& url);

}  // namespace url_utils

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_H_
