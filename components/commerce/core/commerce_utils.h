// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_

#include "components/commerce/core/commerce_types.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace commerce {
// Gets the url for the ProductSpec page based on `urls`.
GURL GetProductSpecsTabUrl(const std::vector<GURL>& urls);

// Gets the url for the ProductSpec page based on `uuid`.
GURL GetProductSpecsTabUrlForID(const base::Uuid& uuid);

// Produce a ProductInfo object given OptimizationGuideMeta. The returned
// unique_ptr is owned by the caller and will be empty if conversion failed
// or there was no info.
std::unique_ptr<ProductInfo> OptGuideResultToProductInfo(
    const optimization_guide::OptimizationMetadata& metadata,
    bool can_load_product_specs_full_page_ui = false);

// Conditionally route traffic to an alternate shopping server by sending
// HTTP headers with the request.
void MaybeUseAlternateShoppingServer(
    endpoint_fetcher::EndpointFetcher::RequestParams::Builder& params_builder);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
