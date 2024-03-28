// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"

namespace commerce {

class ProductSpecificationsSet;

// Acquires synced data about product specifications.
class ProductSpecificationsService : public KeyedService {
 public:
  explicit ProductSpecificationsService(
      std::unique_ptr<ProductSpecificationsSyncBridge> bridge);
  ProductSpecificationsService(const ProductSpecificationsService&) = delete;
  ProductSpecificationsService& operator=(const ProductSpecificationsService&) =
      delete;
  ~ProductSpecificationsService() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

  const std::vector<const ProductSpecificationsSet>
  GetAllProductSpecifications();

 private:
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
