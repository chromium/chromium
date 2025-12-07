// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_URL_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_URL_UTILS_H_

#include <string>

class GURL;

namespace history {

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
