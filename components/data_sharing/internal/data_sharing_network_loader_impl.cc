// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include "components/signin/public/identity_manager/identity_manager.h"

namespace data_sharing {

DataSharingNetworkLoaderImpl::DataSharingNetworkLoaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

DataSharingNetworkLoaderImpl::~DataSharingNetworkLoaderImpl() = default;

void DataSharingNetworkLoaderImpl::LoadUrl(
    const GURL& gurl,
    const std::vector<std::string>& scopes,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    NetworkLoaderCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace data_sharing
