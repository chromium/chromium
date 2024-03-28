// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace commerce {

ProductSpecificationsService::ProductSpecificationsService(
    std::unique_ptr<ProductSpecificationsSyncBridge> bridge)
    : bridge_(std::move(bridge)) {}

ProductSpecificationsService::~ProductSpecificationsService() = default;

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ProductSpecificationsService::GetSyncControllerDelegate() {
  CHECK(bridge_);
  return bridge_->change_processor()->GetControllerDelegate();
}

const std::vector<const ProductSpecificationsSet>
ProductSpecificationsService::GetAllProductSpecifications() {
  std::vector<const ProductSpecificationsSet> product_specifications;
  for (auto& entry : bridge_->entries()) {
    std::vector<const GURL> urls;
    for (auto& data : entry.second.data()) {
      urls.emplace_back(data.url());
    }
    product_specifications.emplace_back(
        entry.second.uuid(), entry.second.creation_time_unix_epoch_micros(),
        entry.second.update_time_unix_epoch_micros(), urls,
        entry.second.name());
  }
  return product_specifications;
}

}  // namespace commerce
