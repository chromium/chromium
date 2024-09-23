// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/candidate_product.h"

namespace commerce {

CandidateProduct::CandidateProduct(const GURL& url,
                                   const ProductInfo& product_info)
    : product_url(url), category_data(product_info.category_data) {}

CandidateProduct::~CandidateProduct() = default;

}  // namespace commerce
