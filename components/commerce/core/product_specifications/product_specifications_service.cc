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

const std::vector<ProductSpecificationsSet>
ProductSpecificationsService::GetAllProductSpecifications() {
  std::vector<ProductSpecificationsSet> product_specifications;
  for (auto& entry : bridge_->entries()) {
    std::vector<GURL> urls;
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

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::GetSetByUuid(const base::Uuid& uuid) {
  // TODO(b:337263623): Consider centralizing ID logic for product
  //                    specifications.
  auto it = bridge_->entries().find(uuid.AsLowercaseString());
  if (it == bridge_->entries().end()) {
    return std::nullopt;
  }

  return ProductSpecificationsSet::FromProto(it->second);
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::AddProductSpecificationsSet(
    const std::string& name,
    const std::vector<GURL>& urls) {
  // TODO(crbug.com/332545064) add for a product specification set being added.
  std::optional<sync_pb::CompareSpecifics> specifics =
      bridge_->AddProductSpecifications(name, urls);
  if (!specifics.has_value()) {
    return std::nullopt;
  }
  return std::optional(ProductSpecificationsSet::FromProto(specifics.value()));
}

std::optional<ProductSpecificationsSet> ProductSpecificationsService::SetUrls(
    const base::Uuid& uuid,
    const std::vector<GURL>& urls) {
  std::optional<ProductSpecificationsSet> product_specs_set =
      GetSetByUuid(uuid);
  if (!product_specs_set.has_value()) {
    return std::nullopt;
  }

  product_specs_set->urls_.clear();
  for (const auto& url : urls) {
    product_specs_set->urls_.push_back(url);
  }

  std::optional<sync_pb::CompareSpecifics> updated_specifics =
      bridge_->UpdateProductSpecificationsSet(product_specs_set.value());
  if (!updated_specifics.has_value()) {
    return std::nullopt;
  }
  return ProductSpecificationsSet::FromProto(updated_specifics.value());
}

std::optional<ProductSpecificationsSet> ProductSpecificationsService::SetName(
    const base::Uuid& uuid,
    const std::string& name) {
  std::optional<ProductSpecificationsSet> product_specs_set =
      GetSetByUuid(uuid);
  if (!product_specs_set.has_value()) {
    return std::nullopt;
  }

  product_specs_set->name_ = name;

  std::optional<sync_pb::CompareSpecifics> updated_specifics =
      bridge_->UpdateProductSpecificationsSet(product_specs_set.value());
  if (!updated_specifics.has_value()) {
    return std::nullopt;
  }
  return ProductSpecificationsSet::FromProto(updated_specifics.value());
}

void ProductSpecificationsService::DeleteProductSpecificationsSet(
    const std::string& uuid) {
  bridge_->DeleteProductSpecificationsSet(uuid);
}

void ProductSpecificationsService::AddObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  bridge_->AddObserver(observer);
}

void ProductSpecificationsService::RemoveObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  bridge_->RemoveObserver(observer);
}

}  // namespace commerce
