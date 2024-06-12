// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace commerce {

// Class for getting product clustering information from the server.
class ClusterServerProxy {
 public:
  using GetComparableProductsCallback =
      base::OnceCallback<void(const std::vector<uint64_t>&)>;

  ClusterServerProxy(
      signin::IdentityManager* identity_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
  virtual ~ClusterServerProxy();
  ClusterServerProxy(const ClusterServerProxy& other) = delete;
  ClusterServerProxy& operator=(const ClusterServerProxy& other) = delete;

  // Given a list of product cluster Ids, find whether some of the products are
  // comparable and return them.
  virtual void GetComparableProducts(
      const std::vector<uint64_t>& product_cluster_ids,
      GetComparableProductsCallback callback);

 protected:
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& post_data);

 private:
  void HandleCompareResponse(GetComparableProductsCallback callback,
                             std::unique_ptr<EndpointFetcher> endpoint_fetcher,
                             std::unique_ptr<EndpointResponse> response);

  void OnResponseJsonParsed(GetComparableProductsCallback callback,
                            data_decoder::DataDecoder::ValueOrError result);

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ClusterServerProxy> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_
