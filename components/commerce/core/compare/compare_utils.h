// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_COMPARE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_COMPARE_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace commerce {

// Returns a json string for a list of product cluster Ids to be sent
// to compare backend.
std::string GetJsonStringForProductClusterIds(
    std::vector<uint64_t> product_cluster_ids);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_COMPARE_UTILS_H_
