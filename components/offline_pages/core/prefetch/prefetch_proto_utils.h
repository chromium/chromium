// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_

#include <string>
#include <vector>
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

// The fully qualified type name for PageBundle defined in proto.
extern const char kPageBundleTypeURL[];

// Parse the Operation serialized in binary proto |data|. Returns the operation
// name if parsing succeeded. Otherwise, empty string is returned.
std::string ParseOperationResponse(const std::string& data,
                                   std::vector<RenderPageInfo>* pages);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PROTO_UTILS_H_
