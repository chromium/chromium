// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <memory>
#include <optional>

#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace commerce {

ProductSpecificationsService::ProductSpecificationsService(
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : bridge_(std::make_unique<ProductSpecificationsSyncBridge>(
          std::move(create_store_callback),
          std::move(change_processor),
          base::BindOnce(&ProductSpecificationsService::OnInit,
                         base::Unretained(this)))) {}

ProductSpecificationsService::~ProductSpecificationsService() = default;

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ProductSpecificationsService::GetSyncControllerDelegate() {
  CHECK(bridge_);
  return bridge_->change_processor()->GetControllerDelegate();
}

const std::vector<ProductSpecificationsSet>
ProductSpecificationsService::GetAllProductSpecifications() {
  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    std::map<std::string, std::vector<sync_pb::ProductComparisonSpecifics>>
        items_lookup;
    // Map product_comparison_uuid to product_comparison_item so the data for
    // each item can be merged the top level specific stored in
    // product_comparison.
    for (auto& [_, specifics] : bridge_->entries()) {
      if (specifics.has_product_comparison_item()) {
        std::string uuid =
            specifics.product_comparison_item().product_comparison_uuid();
        if (!items_lookup.contains(uuid)) {
          items_lookup[uuid] = {};
        }
        items_lookup[uuid].push_back(specifics);
      }
    }
    // Order items, as defined by UniquePosition.
    for (auto& [_, item_specifics] : items_lookup) {
      std::sort(
          item_specifics.begin(), item_specifics.end(),
          [](const auto& specifics_a, const auto& specifics_b) {
            return syncer::UniquePosition::FromProto(
                       specifics_a.product_comparison_item().unique_position())
                .LessThan(syncer::UniquePosition::FromProto(
                    specifics_b.product_comparison_item().unique_position()));
          });
    }
    // Create ProductSpecificationSets.
    std::vector<ProductSpecificationsSet> sets;
    for (auto& [uuid, specifics] : bridge_->entries()) {
      if (specifics.has_product_comparison()) {
        std::vector<GURL> urls;
        if (base::FindOrNull(items_lookup, uuid)) {
          for (auto& specific : items_lookup.find(uuid)->second) {
            urls.emplace_back(specific.product_comparison_item().url());
          }
        }
        sets.emplace_back(specifics.uuid(),
                          specifics.creation_time_unix_epoch_millis(),
                          specifics.update_time_unix_epoch_millis(), urls,
                          specifics.product_comparison().name());
      }
    }
    return sets;
  } else {
    std::vector<ProductSpecificationsSet> product_specifications;
    for (auto& entry : bridge_->entries()) {
      std::vector<GURL> urls;
      for (auto& data : entry.second.data()) {
        urls.emplace_back(data.url());
      }
      product_specifications.emplace_back(
          entry.second.uuid(), entry.second.creation_time_unix_epoch_millis(),
          entry.second.update_time_unix_epoch_millis(), urls,
          entry.second.name());
    }
    return product_specifications;
  }
}

void ProductSpecificationsService::GetAllProductSpecifications(
    GetAllCallback callback) {
  if (is_initialized_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), GetAllProductSpecifications()));
  } else {
    deferred_operations_.push_back(base::BindOnce(
        [](base::WeakPtr<ProductSpecificationsService>
               product_specifications_service,
           GetAllCallback callback) {
          if (product_specifications_service) {
            std::move(callback).Run(product_specifications_service.get()
                                        ->GetAllProductSpecifications());
          } else {
            std::move(callback).Run({});
          }
        },
        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
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

void ProductSpecificationsService::GetSetByUuid(
    const base::Uuid& uuid,
    base::OnceCallback<void(std::optional<ProductSpecificationsSet>)>
        callback) {
  if (is_initialized_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetSetByUuid(uuid)));
  } else {
    deferred_operations_.push_back(base::BindOnce(
        [](base::WeakPtr<ProductSpecificationsService>
               product_specifications_service,
           base::OnceCallback<void(std::optional<ProductSpecificationsSet>)>
               callback,
           const base::Uuid& uuid) {
          if (product_specifications_service) {
            std::move(callback).Run(
                product_specifications_service.get()->GetSetByUuid(uuid));
          } else {
            std::move(callback).Run(std::nullopt);
          }
        },
        weak_ptr_factory_.GetWeakPtr(), std::move(callback), uuid));
  }
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::AddProductSpecificationsSet(
    const std::string& name,
    const std::vector<GURL>& urls) {
  // TODO(crbug.com/332545064) add for a product specification set being added.
  return bridge_->AddProductSpecifications(name, urls);
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetUrls(const base::Uuid& uuid,
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

  std::optional<sync_pb::ProductComparisonSpecifics> updated_specifics =
      bridge_->UpdateProductSpecificationsSet(product_specs_set.value());
  if (!updated_specifics.has_value()) {
    return std::nullopt;
  }
  return ProductSpecificationsSet::FromProto(updated_specifics.value());
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetName(const base::Uuid& uuid,
                                      const std::string& name) {
  std::optional<ProductSpecificationsSet> product_specs_set =
      GetSetByUuid(uuid);
  if (!product_specs_set.has_value()) {
    return std::nullopt;
  }

  product_specs_set->name_ = name;

  std::optional<sync_pb::ProductComparisonSpecifics> updated_specifics =
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

void ProductSpecificationsService::OnInit() {
  is_initialized_ = true;
  for (base::OnceCallback<void(void)>& deferred_operation :
       deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
}

}  // namespace commerce
