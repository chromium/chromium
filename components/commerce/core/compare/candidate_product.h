// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CANDIDATE_PRODUCT_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CANDIDATE_PRODUCT_H_

#include <set>

#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "url/gurl.h"

namespace commerce {
// The data structure to store data for one product that is in a open tab but
// not in any existing product group yet.
struct CandidateProduct {
  CandidateProduct(const GURL& url, const ProductInfo& product_info);
  ~CandidateProduct();

  CandidateProduct(const CandidateProduct&) = delete;
  CandidateProduct& operator=(const CandidateProduct&) = delete;

  // URL of the product.
  GURL product_url;

  // Set of URLs of candidate products that are similar to this product and
  // potentially be clustered into one product group.
  std::set<GURL> similar_candidate_products_urls;

  // Category information about the product.
  CategoryData category_data;
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CANDIDATE_PRODUCT_H_
