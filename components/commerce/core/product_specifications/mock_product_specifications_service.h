// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_MOCK_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_MOCK_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {

class MockProductSpecificationsService : public ProductSpecificationsService {
 public:
  MockProductSpecificationsService();
  ~MockProductSpecificationsService() override;
  MOCK_METHOD(const std::vector<ProductSpecificationsSet>,
              GetAllProductSpecifications,
              (),
              (override));
  MOCK_METHOD(void,
              GetAllProductSpecifications,
              (GetAllCallback callback),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              GetSetByUuid,
              (const base::Uuid& uuid),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              AddProductSpecificationsSet,
              (const std::string& name, const std::vector<UrlInfo>& urls),
              (override));
  MOCK_METHOD(void,
              DeleteProductSpecificationsSet,
              (const std::string& uuid),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              SetName,
              (const base::Uuid& uuid, const std::string& name),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              SetUrls,
              (const base::Uuid& uuid, const std::vector<UrlInfo>& urls),
              (override));
  MOCK_METHOD(void,
              AddObserver,
              (commerce::ProductSpecificationsSet::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (commerce::ProductSpecificationsSet::Observer * observer),
              (override));
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_MOCK_PRODUCT_SPECIFICATIONS_SERVICE_H_
