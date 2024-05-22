// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_cluster_manager.h"

namespace commerce {

MockClusterManager::MockClusterManager(
    ProductSpecificationsService* product_specifications_service)
    : ClusterManager(
          product_specifications_service,
          base::RepeatingCallback<void(const GURL&, ProductInfoCallback)>(),
          base::RepeatingCallback<const std::vector<UrlInfo>()>()) {}

MockClusterManager::~MockClusterManager() = default;

void MockClusterManager::SetResponseForGetEntryPointInfoForSelection(
    std::optional<EntryPointInfo> entry_point_info) {
  ON_CALL(*this, GetEntryPointInfoForSelection)
      .WillByDefault(testing::Return(entry_point_info));
}

void MockClusterManager::SetResponseForGetEntryPointInfoForNavigation(
    std::optional<EntryPointInfo> entry_point_info) {
  ON_CALL(*this, GetEntryPointInfoForNavigation)
      .WillByDefault(testing::Return(entry_point_info));
}

void MockClusterManager::SetResponseForGetProductGroupForCandidateProduct(
    std::optional<ProductGroup> product_group) {
  ON_CALL(*this, GetProductGroupForCandidateProduct)
      .WillByDefault(testing::Return(product_group));
}
}  // namespace commerce
