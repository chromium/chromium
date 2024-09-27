// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"

namespace commerce {

extern const size_t kMaxNameLength;

extern const size_t kMaxTableSize;

class ProductSpecificationsServiceTest;
class ProductSpecificationsServiceSyncDisabledTest;

// Acquires synced data about product specifications.
class ProductSpecificationsService
    : public KeyedService,
      public ProductSpecificationsSyncBridge::Delegate {
 public:
  using GetAllCallback =
      base::OnceCallback<void(const std::vector<ProductSpecificationsSet>)>;
  ProductSpecificationsService(
      syncer::OnceDataTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  ProductSpecificationsService(const ProductSpecificationsService&) = delete;
  ProductSpecificationsService& operator=(const ProductSpecificationsService&) =
      delete;
  ~ProductSpecificationsService() override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate();

  virtual const std::vector<ProductSpecificationsSet>
  GetAllProductSpecifications();

  virtual void GetAllProductSpecifications(GetAllCallback callback);

  virtual const std::optional<ProductSpecificationsSet> GetSetByUuid(
      const base::Uuid& uuid);

  virtual void GetSetByUuid(
      const base::Uuid& uuid,
      base::OnceCallback<void(std::optional<ProductSpecificationsSet>)>
          callback);

  // Add new product specifications set called |name| with product pages
  // corresponding the urls in |url_infos|. Note, title support is being
  // added to ProductSpecificationsService (crbug.com/357561655), although
  // the title field in UrlInfo is currently unused.
  virtual const std::optional<ProductSpecificationsSet>
  AddProductSpecificationsSet(const std::string& name,
                              const std::vector<UrlInfo>& url_infos);

  // Set the URLs for a product specifications set associated with the provided
  // Uuid. If a set with the provided Uuid exists, an updated
  // ProductSpecificationsSet will be returned, otherwise nullopt.
  virtual const std::optional<ProductSpecificationsSet> SetUrls(
      const base::Uuid& uuid,
      const std::vector<UrlInfo>& urls);

  // Set the name for a product specifications set associated with the provided
  // Uuid. If a set with the provided Uuid exists, an updated
  // ProductSpecificationsSet will be returned, otherwise nullopt.
  virtual const std::optional<ProductSpecificationsSet> SetName(
      const base::Uuid& uuid,
      const std::string& name);

  // Deletes product specification set corresponding to identifier |uuid|.
  virtual void DeleteProductSpecificationsSet(const std::string& uuid);

  // Observer monitoring add/remove/update of ProductSpecificationSets.
  virtual void AddObserver(
      commerce::ProductSpecificationsSet::Observer* observer);

  // Remove observer monitoring add/remove/update of ProductSpecificationSets.
  virtual void RemoveObserver(
      commerce::ProductSpecificationsSet::Observer* observer);

 private:
  friend class commerce::ProductSpecificationsServiceTest;
  friend class commerce::ProductSpecificationsServiceSyncDisabledTest;
  std::unique_ptr<ProductSpecificationsSyncBridge> bridge_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::vector<base::OnceCallback<void()>> deferred_operations_;
  base::ObserverList<commerce::ProductSpecificationsSet::Observer> observers_;

  bool is_initialized_{false};

  void OnInit();
  void OnProductSpecificationsSetAdded(
      const ProductSpecificationsSet& product_specifications_set);
  void OnSpecificsAdded(const std::vector<sync_pb::ProductComparisonSpecifics>
                            specifics) override;

  void OnSpecificsUpdated(
      const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                  sync_pb::ProductComparisonSpecifics>>
          before_after_specifics) override;

  void OnSpecificsRemoved(const std::vector<sync_pb::ProductComparisonSpecifics>
                              specifics) override;

  void OnMultiSpecificsChanged(
      const std::vector<sync_pb::ProductComparisonSpecifics> changed_specifics,
      const std::map<std::string, sync_pb::ProductComparisonSpecifics>
          prev_entries) override;

  void NotifyProductSpecificationsAdded(
      const ProductSpecificationsSet& added_set);

  void NotifyProductSpecificationsUpdate(const ProductSpecificationsSet& before,
                                         const ProductSpecificationsSet& after);

  void NotifyProductSpecificationsRemoval(const ProductSpecificationsSet& set);

  void MigrateLegacySpecificsIfApplicable();

  void DisableInitializedForTesting();

  base::WeakPtrFactory<ProductSpecificationsService> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
