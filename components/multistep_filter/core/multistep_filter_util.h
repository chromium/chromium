// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_

#include <string>

class GURL;

namespace multistep_filter {

// Returns the eTLD+1 for `url`. If it doesn't exist,
// returns the host.
std::string GetEtldPlusOne(const GURL& url);

// Returns true if `url` is allowed by the `kMultistepFilterAllowedDomains`
// feature param.
bool IsUrlAllowed(const GURL& url);

// Returns true if `url` and `other` have the same eTLD+1 or host.
bool IsSameDomainOrHost(const GURL& url, const GURL& other);

// Returns true if the `candidate_url` is redundant given the `reference_url`.
// Redundancy is defined as having the same base URL (scheme, host, and
// path) and the candidate's query parameters being a subset of (or identical
// to) the reference URL's query parameters.
//
// For example:
// candidate: "http://example.com/path?a=1"
// reference: "http://example.com/path?a=1&b=2"
// -> Subsumed (returns true)
//
// candidate: "http://example.com/path?a=1&b=2"
// reference: "http://example.com/path?a=1"
// -> NOT Subsumed (returns false)
bool IsUrlSubsumedBy(const GURL& candidate_url, const GURL& reference_url);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
