// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
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
  void LoadUrl(const GURL& gurl,
               const std::vector<std::string>& scopes,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag,
               NetworkLoaderCallback callback) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_NETWORK_LOADER_IMPL_H_
