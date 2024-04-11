// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <optional>

#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/protocol/compare_specifics.pb.h"

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

const std::optional<const ProductSpecificationsSet>
ProductSpecificationsService::AddProductSpecificationsSet(
    const std::string& name,
    const std::vector<const GURL>& urls) {
  // TODO(crbug.com/332545064) add for a product specification set being added.
  std::optional<sync_pb::CompareSpecifics> specifics =
      bridge_->AddProductSpecifications(name, urls);
  if (!specifics.has_value()) {
    return std::nullopt;
  }
  return std::optional(ProductSpecificationsSet::FromProto(specifics.value()));
}

void ProductSpecificationsService::DeleteProductSpecificationsSet(
    const std::string& uuid) {
  bridge_->DeleteProductSpecificationsSet(uuid);
}

void ProductSpecificationsService::AddObserver(
    const commerce::ProductSpecificationsSet::Observer* observer) {
  bridge_->AddObserver(observer);
}

void ProductSpecificationsService::RemoveObserver(
    const commerce::ProductSpecificationsSet::Observer* observer) {
  bridge_->RemoveObserver(observer);
}

}  // namespace commerce
