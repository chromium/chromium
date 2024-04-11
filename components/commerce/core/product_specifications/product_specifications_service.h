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

  // Add new product specifications set called |name| with product pages
  // corresponding to |urls|.
  const std::optional<const ProductSpecificationsSet>
  AddProductSpecificationsSet(const std::string& name,
                              const std::vector<const GURL>& urls);

  // Deletes product specification set corresponding to identifier |uuid|.
  void DeleteProductSpecificationsSet(const std::string& uuid);

  // Observer monitoring add/remove/update of ProductSpecificationSets.
  void AddObserver(
      const commerce::ProductSpecificationsSet::Observer* observer);

  // Remove observer monitoring add/remove/update of ProductSpecificationSets.
  void RemoveObserver(
      const commerce::ProductSpecificationsSet::Observer* observer);

 private:
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
