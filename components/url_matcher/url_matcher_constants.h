// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the URLMatcher component of the Declarative API.

#ifndef COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_
#define COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_

namespace url_matcher {
namespace url_matcher_constants {

// Keys of dictionaries for URL constraints
inline constexpr char kPortsKey[] = "ports";
inline constexpr char kSchemesKey[] = "schemes";
inline constexpr char kHostContainsKey[] = "hostContains";
inline constexpr char kHostEqualsKey[] = "hostEquals";
inline constexpr char kHostPrefixKey[] = "hostPrefix";
inline constexpr char kHostSuffixKey[] = "hostSuffix";
inline constexpr char kHostSuffixPathPrefixKey[] = "hostSuffixPathPrefix";
inline constexpr char kOriginAndPathMatchesKey[] = "originAndPathMatches";
inline constexpr char kPathContainsKey[] = "pathContains";
inline constexpr char kPathEqualsKey[] = "pathEquals";
inline constexpr char kPathPrefixKey[] = "pathPrefix";
inline constexpr char kPathSuffixKey[] = "pathSuffix";
inline constexpr char kQueryContainsKey[] = "queryContains";
inline constexpr char kQueryEqualsKey[] = "queryEquals";
inline constexpr char kQueryPrefixKey[] = "queryPrefix";
inline constexpr char kQuerySuffixKey[] = "querySuffix";
inline constexpr char kURLContainsKey[] = "urlContains";
inline constexpr char kURLEqualsKey[] = "urlEquals";
inline constexpr char kURLMatchesKey[] = "urlMatches";
inline constexpr char kURLPrefixKey[] = "urlPrefix";
inline constexpr char kURLSuffixKey[] = "urlSuffix";
inline constexpr char kCidrBlocksKey[] = "cidrBlocks";

}  // namespace url_matcher_constants
}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_
