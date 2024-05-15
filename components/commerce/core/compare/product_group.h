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
  ProductGroup(const base::Uuid& uuid,
               const std::string& name,
               const std::vector<GURL>& urls,
               base::Time update_time);
  ~ProductGroup();

  ProductGroup(const ProductGroup&);
  ProductGroup& operator=(const ProductGroup&);

  // Unique ID to identify the product group.
  base::Uuid uuid;

  // Name of the product group.
  std::string name;

  // A set storing URLs of products that are currently in the group.
  std::set<GURL> member_products;

  // The time this group was last updated.
  base::Time update_time;

  // Category infos of this group.
  std::vector<CategoryData> categories;
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_GROUP_H_
