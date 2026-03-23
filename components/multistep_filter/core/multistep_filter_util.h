// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_

#include <string>

class GURL;

namespace multistep_filter {

// Returns the eTLD+1 for `url`.
std::string GetEtldPlusOne(const GURL& url);

// Returns true if `url` is allowed by the `kMultistepFilterAllowedDomains`
// feature param.
bool IsUrlAllowed(const GURL& url);

// Returns true if `url` and `other` have the same eTLD+1 or host.
bool IsSameDomainOrHost(const GURL& url, const GURL& other);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
