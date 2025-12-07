// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GOOGLE_URL_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GOOGLE_URL_UTIL_H_

#include <optional>
#include <string>

#include "url/gurl.h"

namespace page_load_metrics {

// TODO(375343470): Merge following Google specific URL checks to
// //components/google/core/common/google_util.h.

// If the given hostname is a google hostname, returns the portion of the
// hostname before the google hostname. Otherwise, returns an unset optional
// value.
//
// For example:
//   https://example.com/foo => returns an unset optional value
//   https://google.com/foo => returns ''
//   https://www.google.com/foo => returns 'www'
//   https://news.google.com/foo => returns 'news'
//   https://a.b.c.google.com/foo => returns 'a.b.c'
std::optional<std::string> GetGoogleHostnamePrefix(const GURL& url);

// Whether the given url has a google hostname.
bool IsGoogleHostname(const GURL& url);

// Whether the given url has a Google Search hostname.
// Examples:
//   https://www.google.com -> true
//   https://www.google.co.jp -> true
//   https://www.google.example.com -> false
//   https://docs.google.com -> false
bool IsGoogleSearchHostname(const GURL& url);

// Determine if the given url has query associated with it. Note that we do
// not check the domain name, but only check the parameters.
bool HasGoogleSearchQuery(const GURL& url);

// Whether a given URL is probably for Google Search, i.e., it has a Google
// Search hostname and is not part of Google Maps.
//
// The motivation here is for that pages that are part of Google Search, e.g.
// Search Results pages or redirector URLs, we should not log from-Google-Search
// stats. We could try to detect only the specific known search URLs here, and
// log navigations to other pages on the Google Search hostname. (For example, a
// search for 'about google' includes a result for
// https://www.google.com/about/). However, we assume these cases are relatively
// uncommon, and we run the risk of logging metrics for some search redirector
// URLs. Thus we choose the more conservative approach of ignoring all URLs on
// known Search hostnames.
//
// The one exception is Google Maps, which we want to be sure to log stats for.
//
// Examples:
//   https://www.google.com/ -> true
//   https://www.google.co.jp/ -> true
//   https://www.google.com/#q=test -> true
//   https://www.google.com/about/ -> true [false positive, but oh well]
//   https://www.google.com/maps -> false
//   https://www.google.com/maps/otherstuff -> false
//   https://www.google.example.com/ -> false
//   https://docs.google.com/ -> false
bool IsProbablyGoogleSearchUrl(const GURL& url);

// Whether the given url is for a Google Search results page. See
// https://docs.google.com/document/d/1jNPZ6Aeh0KV6umw1yZrrkfXRfxWNruwu7FELLx_cpOg/edit
// for additional details.
// Examples:
//   https://www.google.com/#q=test -> true
//   https://www.google.com/search?q=test -> true
//   https://www.google.com/ -> false
//   https://www.google.com/about/ -> false
bool IsGoogleSearchResultUrl(const GURL& url);

// Whether the given url is for a Google home page.
// Examples:
//   https://www.google.com/ -> true
//   https://www.google.com/search/ -> true
//   https://www.google.com/search?q=test -> false
//   https://www.google.com/maps/ -> false
bool IsGoogleSearchHomepageUrl(const GURL& url);

// Whether the given url is a Google Search redirector URL.
bool IsGoogleSearchRedirectorUrl(const GURL& url);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GOOGLE_URL_UTIL_H_
