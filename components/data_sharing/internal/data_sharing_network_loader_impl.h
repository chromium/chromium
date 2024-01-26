// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing {
class DataSharingNetworkLoader;

// The internal implementation of the DataSharingNetworkLoader.
class DataSharingNetworkLoaderImpl : public DataSharingNetworkLoader {
 public:
  DataSharingNetworkLoaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~DataSharingNetworkLoaderImpl() override;

  // Disallow copy/assign.
  DataSharingNetworkLoaderImpl(const DataSharingNetworkLoaderImpl&) = delete;
  DataSharingNetworkLoaderImpl& operator=(const DataSharingNetworkLoaderImpl&) =
      delete;

  // DataSharingNetworkLoader Impl.
  void LoadUrl(const GURL& url,
               const std::vector<std::string>& scopes,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag,
               NetworkLoaderCallback callback) override;

 protected:
  // This method could be overridden in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::vector<std::string>& scopes,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  // Called when response is received
  void OnDownloadComplete(NetworkLoaderCallback callback,
                          std::unique_ptr<EndpointFetcher> fetcher,
                          std::unique_ptr<EndpointResponse> response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  base::WeakPtrFactory<DataSharingNetworkLoaderImpl> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_
