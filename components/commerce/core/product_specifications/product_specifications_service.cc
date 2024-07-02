// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace {

std::string GetSuffix(const std::string& uuid) {
  return base::Base64Encode(base::SHA1HashString(uuid));
}

syncer::UniquePosition GetNextPosition(
    const syncer::UniquePosition& prev_position,
    const std::string& suffix) {
  if (prev_position.IsValid()) {
    return syncer::UniquePosition::After(prev_position, suffix);
  } else {
    return syncer::UniquePosition::InitialPosition(suffix);
  }
}

}  // namespace

namespace commerce {

ProductSpecificationsService::ProductSpecificationsService(
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : bridge_(std::make_unique<ProductSpecificationsSyncBridge>(
          std::move(create_store_callback),
          std::move(change_processor),
          base::BindOnce(&ProductSpecificationsService::OnInit,
                         base::Unretained(this)),
          this)) {}

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
  std::vector<sync_pb::ProductComparisonSpecifics> specifics;
  int64_t time_now = base::Time::Now().InMillisecondsSinceUnixEpoch();
  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    sync_pb::ProductComparisonSpecifics comparison_specifics;
    std::string top_level_uuid =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
    comparison_specifics.set_uuid(top_level_uuid);
    comparison_specifics.set_creation_time_unix_epoch_millis(time_now);
    comparison_specifics.set_update_time_unix_epoch_millis(time_now);
    comparison_specifics.mutable_product_comparison()->set_name(name);
    specifics.push_back(comparison_specifics);
    std::string position_suffix = GetSuffix(top_level_uuid);
    syncer::UniquePosition prev_position;
    for (const GURL& url : urls) {
      sync_pb::ProductComparisonSpecifics item_specifics;
      item_specifics.set_uuid(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
      item_specifics.set_creation_time_unix_epoch_millis(time_now);
      item_specifics.set_update_time_unix_epoch_millis(time_now);
      item_specifics.mutable_product_comparison_item()
          ->set_product_comparison_uuid(top_level_uuid);
      item_specifics.mutable_product_comparison_item()->set_url(url.spec());

      syncer::UniquePosition position =
          GetNextPosition(prev_position, position_suffix);
      *item_specifics.mutable_product_comparison_item()
           ->mutable_unique_position() = position.ToProto();
      prev_position = position;
      specifics.push_back(item_specifics);
    }
    bridge_->AddSpecifics(specifics);
    ProductSpecificationsSet set = ProductSpecificationsSet(
        top_level_uuid, time_now, time_now, urls, name);
    OnProductSpecificationsSetAdded(set);
    return set;
  } else {
    sync_pb::ProductComparisonSpecifics comparison_specifics;
    comparison_specifics.set_uuid(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    comparison_specifics.set_creation_time_unix_epoch_millis(time_now);
    comparison_specifics.set_update_time_unix_epoch_millis(time_now);
    comparison_specifics.set_name(name);
    for (const GURL& url : urls) {
      sync_pb::ComparisonData* comparison_data =
          comparison_specifics.add_data();
      comparison_data->set_url(url.spec());
    }
    bridge_->AddSpecifics({comparison_specifics});
    OnProductSpecificationsSetAdded(
        ProductSpecificationsSet::FromProto(comparison_specifics));
    return ProductSpecificationsSet::FromProto(comparison_specifics);
  }
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
  observers_.AddObserver(observer);
}

void ProductSpecificationsService::RemoveObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  bridge_->RemoveObserver(observer);
  observers_.RemoveObserver(observer);
}

void ProductSpecificationsService::OnInit() {
  is_initialized_ = true;
  for (base::OnceCallback<void(void)>& deferred_operation :
       deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
}

void ProductSpecificationsService::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& product_specifications_set) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetAdded(product_specifications_set);
  }
}

void ProductSpecificationsService::OnSpecificsAdded(
    const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {
  for (const sync_pb::ProductComparisonSpecifics& specific : specifics) {
    // Legacy storage format doesn't use ProductComparison &
    // ProductComparisonItem.
    if (!specific.has_product_comparison() &&
        !specific.has_product_comparison_item()) {
      for (auto& observer : observers_) {
        observer.OnProductSpecificationsSetAdded(
            ProductSpecificationsSet::FromProto(specific));
      }
    }
  }
  // TODO(crbug.com/350346263) Handle new multi specifics format.
}

}  // namespace commerce
