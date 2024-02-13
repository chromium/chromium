// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/commerce_types.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace commerce {

struct ProductSpecifications {
 public:
  typedef uint64_t ProductDimensionId;

  ProductSpecifications();
  ProductSpecifications(const ProductSpecifications&);
  ~ProductSpecifications();

  struct Product {
   public:
    Product();
    Product(const Product&);
    ~Product();

    uint64_t product_cluster_id;
    std::string mid;
    std::string title;
    GURL image_url;
    std::map<ProductDimensionId, std::vector<std::string>>
        product_dimension_values;
  };

  // A map of each product dimension ID to its human readable name.
  std::map<ProductDimensionId, std::string> product_dimension_map;

  // The list of products in the specification group.
  std::vector<Product> products;
};

class ProductSpecificationsServerProxy {
 public:
  ProductSpecificationsServerProxy();
  ProductSpecificationsServerProxy(const ProductSpecificationsServerProxy&) =
      delete;
  ProductSpecificationsServerProxy operator=(
      const ProductSpecificationsServerProxy&) = delete;
  ~ProductSpecificationsServerProxy();

  // Gets the specifications data for the provided cluster IDs. The callback
  // will provide both the list of product cluster IDs for the products being
  // compared and the specifications data.
  void GetProductSpecificationsForClusterIds(
      std::vector<uint64_t> cluster_ids,
      base::OnceCallback<void(std::vector<uint64_t>, ProductSpecifications)>);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProductSpecificationsServerProxyTest,
                           JsonToProductSpecifications);

  // Returns a ProductSpecifications object for the provided JSON. If the JSON
  // cannot be converted, std::nullopt is returned.
  static std::optional<ProductSpecifications>
  ProductSpecificationsFromJsonResponse(const base::Value& compareJson);

  base::WeakPtrFactory<ProductSpecificationsServerProxy> weak_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_
