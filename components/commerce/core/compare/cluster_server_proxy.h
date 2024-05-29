// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_

#include <set>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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
  using GetComparableUrlsCallback =
      base::OnceCallback<void(bool /*success*/, const std::set<GURL>&)>;

  ClusterServerProxy(
      signin::IdentityManager* identity_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
  virtual ~ClusterServerProxy();
  ClusterServerProxy(const ClusterServerProxy& other) = delete;
  ClusterServerProxy& operator=(const ClusterServerProxy& other) = delete;

  // Given a list of URLs, find whether some of the URLs are comparable and
  // return them.
  virtual void GetComparableUrls(const std::set<GURL>& product_urls,
                                 GetComparableUrlsCallback callback);

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ClusterServerProxy> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_SERVER_PROXY_H_
