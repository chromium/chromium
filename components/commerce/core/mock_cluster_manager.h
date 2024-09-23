// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_CLUSTER_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_CLUSTER_MANAGER_H_

#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/cluster_manager.h"
#include "components/commerce/core/compare/product_group.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {

class ProductSpecificationsService;

class MockClusterManager : public ClusterManager {
 public:
  explicit MockClusterManager(
      ProductSpecificationsService* product_specifications_service);
  ~MockClusterManager() override;

  MOCK_METHOD(void,
              GetEntryPointInfoForNavigation,
              (const GURL& url,
               ClusterManager::GetEntryPointInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetEntryPointInfoForSelection,
              (const GURL& old_url,
               const GURL& new_url,
               ClusterManager::GetEntryPointInfoCallback callback),
              (override));
  MOCK_METHOD(std::optional<ProductGroup>,
              GetProductGroupForCandidateProduct,
              (const GURL& product_url),
              (override));
  MOCK_METHOD(void,
              GetComparableProducts,
              (const EntryPointInfo& entry_point_info,
               ClusterManager::GetEntryPointInfoCallback callback),
              (override));

  void SetResponseForGetEntryPointInfoForSelection(
      std::optional<EntryPointInfo> entry_point_info);
  void SetResponseForGetEntryPointInfoForNavigation(
      std::optional<EntryPointInfo> entry_point_info);
  void SetResponseForGetProductGroupForCandidateProduct(
      std::optional<ProductGroup> product_group);
  void SetResponseForGetComparableProducts(
      std::optional<EntryPointInfo> entry_point_info);
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_CLUSTER_MANAGER_H_
