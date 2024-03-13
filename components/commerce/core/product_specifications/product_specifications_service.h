// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace commerce {

// Acquires synced data about product specifications.
class ProductSpecificationsService : public KeyedService {
 public:
  ProductSpecificationsService();
  ProductSpecificationsService(const ProductSpecificationsService&) = delete;
  ProductSpecificationsService& operator=(const ProductSpecificationsService&) =
      delete;

  ~ProductSpecificationsService() override;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
