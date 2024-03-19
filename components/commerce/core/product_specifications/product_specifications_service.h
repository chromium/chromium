// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace commerce {

// Acquires synced data about product specifications.
class ProductSpecificationsService : public KeyedService {
 public:
  ProductSpecificationsService(
      std::unique_ptr<ProductSpecificationsSyncBridge> bridge,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ProductSpecificationsService(const ProductSpecificationsService&) = delete;
  ProductSpecificationsService& operator=(const ProductSpecificationsService&) =
      delete;
  ~ProductSpecificationsService() override;

  // Instantiates a controller delegate to interact with
  // ProductSpecificationsSyncBridge. Must be called from the UI thread.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  CreateSyncControllerDelegate();

 private:
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
