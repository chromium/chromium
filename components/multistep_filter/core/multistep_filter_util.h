// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_

class GURL;

namespace multistep_filter {

// Returns true if the given URL is allowed by the
// `kMultistepFilterAllowedDomains` feature param.
bool IsUrlAllowed(const GURL& url);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UTIL_H_
