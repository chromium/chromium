// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_UTIL_H_
#define COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_UTIL_H_

#include <string>

#include "components/no_state_prefetch/common/prerender_types.mojom.h"

class GURL;

namespace prerender {
extern const char kFollowOnlyWhenPrerenderShown[];

// Returns true iff the scheme of the URL given is valid for prerendering.
bool DoesURLHaveValidScheme(const GURL& url);

// Returns true iff the scheme of the subresource URL given is valid for
// prerendering.
bool DoesSubresourceURLHaveValidScheme(const GURL& url);

// Returns true iff the method given is valid for prerendering.
bool IsValidHttpMethod(prerender::mojom::PrerenderMode prerender_mode,
                       const std::string& method);

std::string ComposeHistogramName(const std::string& prefix_type,
                                 const std::string& name);

// Called when a NoStatePrefetch request has received a response (including
// redirects). May be called several times per resource, in case of redirects.
void RecordPrefetchResponseReceived(const std::string& histogram_prefix,
                                    bool is_main_resource,
                                    bool is_redirect,
                                    bool is_no_store);

// Called when a NoStatePrefetch resource has been loaded. This is called only
// once per resource, when all redirects have been resolved.
void RecordPrefetchRedirectCount(const std::string& histogram_prefix,
                                 bool is_main_resource,
                                 int redirect_count);

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_UTIL_H_
