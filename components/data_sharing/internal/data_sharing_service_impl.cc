// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

namespace data_sharing {

DataSharingServiceImpl::DataSharingServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : data_sharing_network_loader_(
          std::make_unique<DataSharingNetworkLoaderImpl>(url_loader_factory,
                                                         identity_manager)) {}

DataSharingServiceImpl::~DataSharingServiceImpl() = default;

bool DataSharingServiceImpl::IsEmptyService() {
  return false;
}

DataSharingNetworkLoader*
DataSharingServiceImpl::GetDataSharingNetworkLoader() {
  return data_sharing_network_loader_.get();
}

}  // namespace data_sharing
