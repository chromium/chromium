// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_

#include <map>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/commerce_types.h"
#include "url/gurl.h"

class EndpointFetcher;
struct EndpointResponse;

namespace base {
class Value;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace commerce {

class AccountChecker;

class ProductSpecificationsServerProxy {
 public:
  ProductSpecificationsServerProxy(
      AccountChecker* account_checker,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ProductSpecificationsServerProxy(const ProductSpecificationsServerProxy&) =
      delete;
  ProductSpecificationsServerProxy operator=(
      const ProductSpecificationsServerProxy&) = delete;
  virtual ~ProductSpecificationsServerProxy();

  // Gets the specifications data for the provided cluster IDs. The callback
  // will provide both the list of product cluster IDs for the products being
  // compared and the specifications data.
  virtual void GetProductSpecificationsForClusterIds(
      std::vector<uint64_t> cluster_ids,
      ProductSpecificationsCallback callback);

 protected:
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& http_method,
      const std::string& post_data);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProductSpecificationsServerProxyTest,
                           JsonToProductSpecifications);

  // Returns a ProductSpecifications object for the provided JSON. If the JSON
  // cannot be converted, std::nullopt is returned.
  static std::optional<ProductSpecifications>
  ProductSpecificationsFromJsonResponse(const base::Value& compareJson);

  void HandleSpecificationsResponse(
      std::vector<uint64_t> cluster_ids,
      base::OnceCallback<void(std::vector<uint64_t>,
                              std::optional<ProductSpecifications>)> callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  raw_ptr<AccountChecker> account_checker_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ProductSpecificationsServerProxy> weak_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_PRODUCT_SPECIFICATIONS_SERVER_PROXY_H_
