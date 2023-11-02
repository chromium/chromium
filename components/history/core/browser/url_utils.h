// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_URL_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_URL_UTILS_H_

#include <string>

class GURL;

namespace history {

// CanonicalURLStringCompare performs lexicographical comparison of two strings
// that represent valid URLs, so that if the pre-path (scheme, host, and port)
// parts are equal, then the path parts are compared by treating path components
// (delimited by "/") as separate tokens that form units of comparison.
// For example, let us compare `s1` and `s2`, with
//   `s1` = "http://www.google.com:80/base/test/ab/cd?query/stuff"
//   `s2` = "http://www.google.com:80/base/test-case/yz#ref/stuff"
// The pre-path parts "http://www.google.com:80/" match. We treat the paths as
//   `s1` => ["base", "test", "ab", "cd"]
//   `s2` => ["base", "test-case", "yz"]
// Components 1 "base" are identical. Components 2 yield "test" < "test-case",
// so we consider `s1` < `s2`, and return true. Note that naive string
// comparison would yield the opposite (`s1` > `s2`), since '/' > '-' in ASCII.
// Note that path can be terminated by "?query" or "#ref". The post-path parts
// are compared in an arbitrary (but consistent) way.
bool CanonicalURLStringCompare(const std::string& s1, const std::string& s2);

// Returns whether `url1` and `url2` have the same scheme, host, and port.
bool HaveSameSchemeHostAndPort(const GURL&url1, const GURL& url2);

// Treats `path1` and `path2` as lists of path components (e.g., ["a", "bb"]
// for "/a/bb"). Returns whether `path1`'s list is a prefix of `path2`'s list.
// This is used to define "URL prefix". Note that "test" does not count as a
// prefix of "testing", even though "test" is a (string) prefix of "testing".
bool IsPathPrefix(const std::string& p1, const std::string& p2);

// Converts `url` from HTTP to HTTPS, and vice versa, then returns the result.
// If `url` is neither HTTP nor HTTPS, returns an empty URL.
GURL ToggleHTTPAndHTTPS(const GURL& url);

// Returns the host of the given URL, canonicalized as it would be for
// HistoryService::TopHosts().
std::string HostForTopHosts(const GURL& url);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_URL_UTILS_H_
