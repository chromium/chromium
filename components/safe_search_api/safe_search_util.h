// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_UTIL_H_
#define COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_UTIL_H_

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace safe_search_api {

// Parameters that get appended to force SafeSearch.
extern const char kSafeSearchSafeParameter[];
extern const char kSafeSearchSsuiParameter[];

// Headers set for restricted YouTube.
extern const char kYouTubeRestrictHeaderName[];
extern const char kYouTubeRestrictHeaderValueModerate[];
extern const char kYouTubeRestrictHeaderValueStrict[];

// Header set when restricting allowed domains for apps.
extern const char kGoogleAppsAllowedDomains[];

// Values for YouTube Restricted Mode.
// VALUES MUST COINCIDE WITH ForceYouTubeRestrict POLICY.
enum YouTubeRestrictMode {
  YOUTUBE_RESTRICT_OFF = 0,       // Do not restrict YouTube content. YouTube
                                  // might still restrict content based on its
                                  // user settings.
  YOUTUBE_RESTRICT_MODERATE = 1,  // Enforce at least a moderately strict
                                  // content filter for YouTube.
  YOUTUBE_RESTRICT_STRICT = 2,  // Enforce a strict content filter for YouTube.
  YOUTUBE_RESTRICT_COUNT = 3    // Enum counter
};

// If |url| is a url to Google Web Search, enforces that the SafeSearch
// query parameters are set to active. Sets |new_url| to a copy of the request
// url in which the query part contains the new values of the parameters.
void ForceGoogleSafeSearch(const GURL& url, GURL* new_url);

// Does nothing if |url| is not a url to YouTube. Otherwise, if |mode|
// is not |YOUTUBE_RESTRICT_OFF|, enforces a minimum YouTube Restrict mode
// by setting YouTube Restrict header. Setting |YOUTUBE_RESTRICT_OFF| is not
// supported and will do nothing in production.
void ForceYouTubeRestrict(const GURL& url,
                          net::HttpRequestHeaders* headers,
                          YouTubeRestrictMode mode);

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_UTIL_H_
