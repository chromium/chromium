// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PROCESSING_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PROCESSING_UTIL_H_

#include <string>

#include "components/optimization_guide/proto/hints.pb.h"

class GURL;

namespace optimization_guide {

// Returns the string representation of the optimization type.
std::string GetStringNameForOptimizationType(
    proto::OptimizationType optimization_type);

// Returns the matching PageHint for |gurl| if found in |hint|.
const proto::PageHint* FindPageHintForURL(const GURL& gurl,
                                          const proto::Hint* hint);

// The host is hashed and returned as a string because
// base::Value(base::Value::Type::DICT) only accepts strings as keys.
// Note, some hash collisions could occur on hosts. For querying the blocklist,
// collisions are acceptable as they would only block additional hosts. For
// updating the blocklist, a collision would enable a site that should remain on
// the blocklist. However, the likelihood of a collision for the number of hosts
// allowed in the blocklist is practically zero.
std::string HashHostForDictionary(const std::string& host);

// Validates a URL used for URL-keyed hints.
bool IsValidURLForURLKeyedHint(const GURL& url);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PROCESSING_UTIL_H_
