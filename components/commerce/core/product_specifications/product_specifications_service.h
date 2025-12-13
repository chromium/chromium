// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/keyed_service/core/keyed_service.h"

namespace commerce {

extern const size_t kMaxNameLength;

extern const size_t kMaxTableSize;

// Acquires synced data about product specifications.
class ProductSpecificationsService : public KeyedService {
 public:
  using GetAllCallback =
      base::OnceCallback<void(const std::vector<ProductSpecificationsSet>)>;
  ProductSpecificationsService();
  ProductSpecificationsService(const ProductSpecificationsService&) = delete;
  ProductSpecificationsService& operator=(const ProductSpecificationsService&) =
      delete;
  ~ProductSpecificationsService() override;

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
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_H_
