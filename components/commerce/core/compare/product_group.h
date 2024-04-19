// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_GROUP_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_GROUP_H_

#include <set>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "url/gurl.h"

namespace commerce {
// Data structure to store information about an existing group
// of similar products.
struct ProductGroup {
  ProductGroup(const base::Uuid& uuid, const std::vector<GURL>& urls);
  ~ProductGroup();

  ProductGroup(const ProductGroup&) = delete;
  ProductGroup& operator=(const ProductGroup&) = delete;

  // Unique ID to identify the product group.
  base::Uuid uuid;

  // A set storing URLs of products that are currently in the group.
  std::set<GURL> member_products;

  // A set storing URLs of products that are open and comparable to the
  // product group, but is not currently in the group.
  std::set<GURL> candidate_products;

  // Category infos of this group.
  std::vector<CategoryData> categories;
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_GROUP_H_
