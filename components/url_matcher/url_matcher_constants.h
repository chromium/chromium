// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the URLMatcher component of the Declarative API.

#ifndef COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_
#define COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_

#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {
namespace url_matcher_constants {

// Keys of dictionaries for URL constraints
URL_MATCHER_EXPORT extern const char kPortsKey[];
URL_MATCHER_EXPORT extern const char kSchemesKey[];
URL_MATCHER_EXPORT extern const char kHostContainsKey[];
URL_MATCHER_EXPORT extern const char kHostEqualsKey[];
URL_MATCHER_EXPORT extern const char kHostPrefixKey[];
URL_MATCHER_EXPORT extern const char kHostSuffixKey[];
URL_MATCHER_EXPORT extern const char kHostSuffixPathPrefixKey[];
URL_MATCHER_EXPORT extern const char kOriginAndPathMatchesKey[];
URL_MATCHER_EXPORT extern const char kPathContainsKey[];
URL_MATCHER_EXPORT extern const char kPathEqualsKey[];
URL_MATCHER_EXPORT extern const char kPathPrefixKey[];
URL_MATCHER_EXPORT extern const char kPathSuffixKey[];
URL_MATCHER_EXPORT extern const char kQueryContainsKey[];
URL_MATCHER_EXPORT extern const char kQueryEqualsKey[];
URL_MATCHER_EXPORT extern const char kQueryPrefixKey[];
URL_MATCHER_EXPORT extern const char kQuerySuffixKey[];
URL_MATCHER_EXPORT extern const char kURLContainsKey[];
URL_MATCHER_EXPORT extern const char kURLEqualsKey[];
URL_MATCHER_EXPORT extern const char kURLMatchesKey[];
URL_MATCHER_EXPORT extern const char kURLPrefixKey[];
URL_MATCHER_EXPORT extern const char kURLSuffixKey[];
URL_MATCHER_EXPORT extern const char kCidrBlocksKey[];

}  // namespace url_matcher_constants
}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_URL_MATCHER_CONSTANTS_H_
