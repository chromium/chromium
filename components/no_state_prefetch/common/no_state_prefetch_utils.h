// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_UTILS_H_
#define COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_UTILS_H_

#include <string>

class GURL;

namespace prerender {

extern const char kFollowOnlyWhenPrerenderShown[];

// Returns true iff the scheme of the URL given is valid for prefetch.
bool DoesURLHaveValidScheme(const GURL& url);

// Returns true iff the scheme of the subresource URL given is valid for
// prefetch.
bool DoesSubresourceURLHaveValidScheme(const GURL& url);

// Returns true iff the method given is valid for NoStatePrefetch.
bool IsValidHttpMethod(const std::string& method);

std::string ComposeHistogramName(const std::string& prefix_type,
                                 const std::string& name);

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_UTILS_H_
