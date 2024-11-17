// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace {

syncer::UniquePosition::Suffix GetSuffix(const std::string& uuid) {
  std::string suffix_str = base::Base64Encode(base::SHA1HashString(uuid));
  syncer::UniquePosition::Suffix suffix;
  CHECK_EQ(suffix.size(), suffix_str.size());
  base::ranges::copy(suffix_str, suffix.begin());
  return suffix;
}

syncer::UniquePosition GetNextPosition(
    const syncer::UniquePosition& prev_position,
    const syncer::UniquePosition::Suffix& suffix) {
  if (prev_position.IsValid()) {
    return syncer::UniquePosition::After(prev_position, suffix);
  } else {
    return syncer::UniquePosition::InitialPosition(suffix);
  }
}

const sync_pb::ProductComparisonSpecifics* GetTopLevelSpecific(
    const std::string& uuid,
    const std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries) {
  for (auto& [entry_uuid, specific] : entries) {
    if (specific.has_product_comparison() && entry_uuid == uuid) {
      return &specific;
    }
  }
  return nullptr;
}

std::vector<sync_pb::ProductComparisonSpecifics> GetItemSpecifics(
    const std::string& uuid,
    const std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries) {
  std::vector<sync_pb::ProductComparisonSpecifics> items;
  for (auto& [entry_uuid, specific] : entries) {
    if (specific.has_product_comparison_item() &&
        specific.product_comparison_item().product_comparison_uuid() == uuid) {
      items.push_back(specific);
    }
  }
  return items;
}

std::vector<sync_pb::ProductComparisonSpecifics> CreateItemLevelSpecifics(
    const std::string& top_level_uuid,
    const std::vector<commerce::UrlInfo>& url_infos,
    const base::Time& now) {
  std::vector<sync_pb::ProductComparisonSpecifics> item_level_specifics;
  syncer::UniquePosition::Suffix position_suffix = GetSuffix(top_level_uuid);
  syncer::UniquePosition prev_position;
  for (const auto& url_info : url_infos) {
    sync_pb::ProductComparisonSpecifics new_item;
    new_item.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
    new_item.set_creation_time_unix_epoch_millis(
        now.InMillisecondsSinceUnixEpoch());
    new_item.set_update_time_unix_epoch_millis(
        now.InMillisecondsSinceUnixEpoch());
    new_item.mutable_product_comparison_item()->set_url(url_info.url.spec());
    // Title additions will be phased in over several CLs and we don't want to
    // start saving/syncing until all changes are landed.
    if (base::FeatureList::IsEnabled(
            commerce::kProductSpecificationsSyncTitle)) {
      new_item.mutable_product_comparison_item()->set_title(
          base::UTF16ToUTF8(url_info.title));
    }
    new_item.mutable_product_comparison_item()->set_product_comparison_uuid(
        top_level_uuid);
    syncer::UniquePosition position =
        GetNextPosition(prev_position, position_suffix);
    *new_item.mutable_product_comparison_item()->mutable_unique_position() =
        position.ToProto();
    prev_position = position;
    item_level_specifics.push_back(new_item);
  }
  return item_level_specifics;
}

void SortItemSpecifics(
    std::vector<sync_pb::ProductComparisonSpecifics>& item_specifics) {
  base::ranges::sort(
      item_specifics, [](const auto& specifics_a, const auto& specifics_b) {
        return syncer::UniquePosition::FromProto(
                   specifics_a.product_comparison_item().unique_position())
            .LessThan(syncer::UniquePosition::FromProto(
                specifics_b.product_comparison_item().unique_position()));
      });
}

std::optional<commerce::ProductSpecificationsSet>
GetProductSpecificationsSetFromMultiSpecifics(
    const base::Uuid& uuid,
    const std::map<std::string, sync_pb::ProductComparisonSpecifics>& entries) {
  const sync_pb::ProductComparisonSpecifics* top_level_specific =
      GetTopLevelSpecific(uuid.AsLowercaseString(), entries);
  // If we can't find the top level entry (perhaps due to a sync failure -
  // item level entries were synced, but the top level entry sync failed, the
  // item level entries are considered orphaned and the
  // ProductSpecificationsSet does not exist until the top level entry is
  // synced.
  if (!top_level_specific) {
    return std::nullopt;
  }
  std::vector<sync_pb::ProductComparisonSpecifics> item_specifics =
      GetItemSpecifics(uuid.AsLowercaseString(), entries);
  SortItemSpecifics(item_specifics);

  std::vector<commerce::UrlInfo> url_infos;
  // The time the ProductSpecificationsSet was last updated is that recorded
  // on the latest specific - be it an item level or top level.
  long update_time = top_level_specific->update_time_unix_epoch_millis();
  for (const auto& item_specific : item_specifics) {
    std::u16string title = u"";
    // Title additions will be phased in over several CLs and we don't want to
    // start saving/syncing until all changes are landed.
    if (base::FeatureList::IsEnabled(
            commerce::kProductSpecificationsSyncTitle)) {
      title =
          base::UTF8ToUTF16(item_specific.product_comparison_item().title());
    }
    url_infos.emplace_back(GURL(item_specific.product_comparison_item().url()),
                           title);
    if (update_time < item_specific.update_time_unix_epoch_millis()) {
      update_time = item_specific.update_time_unix_epoch_millis();
    }
  }
  // TODO(crbug.com/353255087) Investigate if ProductSpecificationsSet
  // should take a const ref.
  return commerce::ProductSpecificationsSet(
      top_level_specific->uuid(),
      top_level_specific->creation_time_unix_epoch_millis(), update_time,
      url_infos, top_level_specific->product_comparison().name());
}

std::vector<GURL> GetUrls(const std::vector<commerce::UrlInfo> url_infos) {
  std::vector<GURL> urls;
  for (const auto& url_info : url_infos) {
    urls.push_back(url_info.url);
  }
  return urls;
}

std::vector<commerce::UrlInfo> GetUrlInfos(std::vector<GURL> urls) {
  std::vector<commerce::UrlInfo> url_infos;
  for (const auto& url : urls) {
    url_infos.emplace_back(url, std::u16string());
  }
  return url_infos;
}

}  // namespace

namespace commerce {

const size_t kMaxNameLength = 64;
const size_t kMaxTableSize = 10;

ProductSpecificationsService::ProductSpecificationsService(
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : bridge_(std::make_unique<ProductSpecificationsSyncBridge>(
          std::move(create_store_callback),
          std::move(change_processor),
          base::BindOnce(&ProductSpecificationsService::OnInit,
                         base::Unretained(this)),
          this)) {}

ProductSpecificationsService::~ProductSpecificationsService() = default;

base::WeakPtr<syncer::DataTypeControllerDelegate>
ProductSpecificationsService::GetSyncControllerDelegate() {
  CHECK(bridge_);
  return bridge_->change_processor()->GetControllerDelegate();
}

const std::vector<ProductSpecificationsSet>
ProductSpecificationsService::GetAllProductSpecifications() {
  if (!is_initialized_) {
    return {};
  }

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
      SortItemSpecifics(item_specifics);
    }
    // Create ProductSpecificationSets.
    std::vector<ProductSpecificationsSet> sets;
    for (auto& [uuid, specifics] : bridge_->entries()) {
      if (specifics.has_product_comparison()) {
        std::vector<UrlInfo> url_infos;
        if (base::FindOrNull(items_lookup, uuid)) {
          for (auto& specific : items_lookup.find(uuid)->second) {
            std::u16string title = u"";
            // Title additions will be phased in over several CLs and we don't
            // want to start saving/syncing until all changes are landed.

            if (base::FeatureList::IsEnabled(
                    commerce::kProductSpecificationsSyncTitle)) {
              title =
                  base::UTF8ToUTF16(specific.product_comparison_item().title());
            }
            url_infos.emplace_back(
                GURL(specific.product_comparison_item().url()), title);
          }
        }
        sets.emplace_back(specifics.uuid(),
                          specifics.creation_time_unix_epoch_millis(),
                          specifics.update_time_unix_epoch_millis(), url_infos,
                          specifics.product_comparison().name());
      }
    }
    return sets;
  } else {
    std::vector<ProductSpecificationsSet> product_specifications;
    for (auto& entry : bridge_->entries()) {
      // Specifics with ProductComparison or ProductComparisonItem follow a
      // different format where the ProductSpecificationsSet is stored across
      // multiple specifics. Skip over them for the single specifics case.
      if (entry.second.has_product_comparison() ||
          entry.second.has_product_comparison_item()) {
        continue;
      }
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
  if (!is_initialized_) {
    return std::nullopt;
  }

  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    return GetProductSpecificationsSetFromMultiSpecifics(uuid,
                                                         bridge_->entries());
  }
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
    const std::vector<UrlInfo>& url_infos) {
  if (!is_initialized_) {
    return std::nullopt;
  }

  // Don't allow names that are too long.
  std::string final_name = name;
  if (name.length() > kMaxNameLength) {
    final_name = name.substr(0, kMaxNameLength);
  }

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
    comparison_specifics.mutable_product_comparison()->set_name(final_name);
    specifics.push_back(comparison_specifics);
    base::Time now = base::Time::Now();

    // Truncate to 10 URLs if we're over the max.
    std::vector<UrlInfo> final_url_infos;
    for (size_t i = 0; i < url_infos.size() && i < kMaxTableSize; ++i) {
      final_url_infos.push_back(url_infos[i]);
    }

    std::vector<sync_pb::ProductComparisonSpecifics> item_specifics =
        CreateItemLevelSpecifics(top_level_uuid, final_url_infos, now);
    specifics.insert(specifics.end(),
                     std::make_move_iterator(item_specifics.begin()),
                     std::make_move_iterator(item_specifics.end()));
    bridge_->AddSpecifics(specifics);
    ProductSpecificationsSet set = ProductSpecificationsSet(
        top_level_uuid, time_now, time_now, final_url_infos, final_name);
    OnProductSpecificationsSetAdded(set);
    return set;
  } else {
    sync_pb::ProductComparisonSpecifics comparison_specifics;
    comparison_specifics.set_uuid(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    comparison_specifics.set_creation_time_unix_epoch_millis(time_now);
    comparison_specifics.set_update_time_unix_epoch_millis(time_now);
    comparison_specifics.set_name(final_name);
    size_t current_url_count = 0;
    for (const GURL& url : GetUrls(url_infos)) {
      sync_pb::ComparisonData* comparison_data =
          comparison_specifics.add_data();
      comparison_data->set_url(url.spec());
      current_url_count++;

      // Truncate the URL count at the max.
      if (current_url_count >= kMaxTableSize) {
        break;
      }
    }
    bridge_->AddSpecifics({comparison_specifics});
    OnProductSpecificationsSetAdded(
        ProductSpecificationsSet::FromProto(comparison_specifics));
    return ProductSpecificationsSet::FromProto(comparison_specifics);
  }
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetUrls(const base::Uuid& uuid,
                                      const std::vector<UrlInfo>& url_infos) {
  if (!is_initialized_) {
    return std::nullopt;
  }

  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    const sync_pb::ProductComparisonSpecifics* top_level_specific =
        GetTopLevelSpecific(uuid.AsLowercaseString(), bridge_->entries());
    if (!top_level_specific) {
      return std::nullopt;
    }
    std::optional<ProductSpecificationsSet> previous_set = GetSetByUuid(uuid);
    if (!previous_set.has_value()) {
      return std::nullopt;
    }
    std::vector<sync_pb::ProductComparisonSpecifics> specifics_to_remove =
        GetItemSpecifics(uuid.AsLowercaseString(), bridge_->entries());

    base::Time now = base::Time::Now();
    bridge_->DeleteSpecifics(specifics_to_remove);

    // Truncate to 10 URLs if we're over the max.
    std::vector<UrlInfo> final_url_infos;
    for (size_t i = 0; i < url_infos.size() && i < kMaxTableSize; ++i) {
      final_url_infos.push_back(url_infos[i]);
    }

    // SetUrls has not been updated to include title yet, so use
    // GetUrlInfos(...) to convert GURLs -> UrlInfos with a blank title.
    bridge_->AddSpecifics(CreateItemLevelSpecifics(uuid.AsLowercaseString(),
                                                   final_url_infos, now));
    ProductSpecificationsSet updated_set(
        uuid.AsLowercaseString(),
        top_level_specific->creation_time_unix_epoch_millis(),
        now.InMillisecondsSinceUnixEpoch(), final_url_infos,
        previous_set->name());
    NotifyProductSpecificationsUpdate(previous_set.value(), updated_set);
    return updated_set;
  } else {
    auto entry = bridge_->entries().find(uuid.AsLowercaseString());

    if (entry == bridge_->entries().end()) {
      return std::nullopt;
    }
    sync_pb::ProductComparisonSpecifics original = entry->second;
    sync_pb::ProductComparisonSpecifics& specifics = entry->second;
    specifics.clear_data();

    size_t current_url_count = 0;
    for (const UrlInfo& url_info : url_infos) {
      sync_pb::ComparisonData* data = specifics.add_data();
      data->set_url(url_info.url.spec());
      current_url_count++;

      // Truncate the URL count at the max.
      if (current_url_count >= kMaxTableSize) {
        break;
      }
    }
    specifics.set_update_time_unix_epoch_millis(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    bridge_->UpdateSpecifics(specifics);
    ProductSpecificationsSet set =
        ProductSpecificationsSet::FromProto(specifics);
    NotifyProductSpecificationsUpdate(
        ProductSpecificationsSet::FromProto(original), set);
    return set;
  }
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetName(const base::Uuid& uuid,
                                      const std::string& name) {
  if (!is_initialized_) {
    return std::nullopt;
  }

  // Don't allow names that are too long.
  std::string final_name = name;
  if (name.length() > kMaxNameLength) {
    final_name = name.substr(0, kMaxNameLength);
  }

  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    // If we can't find the top level entry (perhaps due to a sync failure -
    // item level entries were synced, but the top level entry sync failed, the
    // item level entries are considered orphaned and the
    // ProductSpecificationsSet does not exist until the top level entry is
    // synced.
    if (bridge_->entries().find(uuid.AsLowercaseString()) ==
        bridge_->entries().end()) {
      return std::nullopt;
    }
    std::optional<ProductSpecificationsSet> previous_set = GetSetByUuid(uuid);
    if (!previous_set.has_value()) {
      return std::nullopt;
    }
    sync_pb::ProductComparisonSpecifics top_level_specific =
        *GetTopLevelSpecific(uuid.AsLowercaseString(), bridge_->entries());
    top_level_specific.set_name(final_name);
    base::Time now = base::Time::Now();
    top_level_specific.mutable_product_comparison()->set_name(final_name);
    top_level_specific.set_update_time_unix_epoch_millis(
        now.InMillisecondsSinceUnixEpoch());
    bridge_->UpdateSpecifics(top_level_specific);
    std::vector<GURL> urls;
    std::vector<sync_pb::ProductComparisonSpecifics> item_specifics =
        GetItemSpecifics(uuid.AsLowercaseString(), bridge_->entries());
    SortItemSpecifics(item_specifics);
    for (const sync_pb::ProductComparisonSpecifics& specifics :
         item_specifics) {
      urls.emplace_back(specifics.product_comparison_item().url());
    }
    ProductSpecificationsSet updated_set(
        uuid.AsLowercaseString(),
        top_level_specific.creation_time_unix_epoch_millis(),
        now.InMillisecondsSinceUnixEpoch(), urls, final_name);
    NotifyProductSpecificationsUpdate(previous_set.value(), updated_set);
    return updated_set;
  } else {
    auto entry = bridge_->entries().find(uuid.AsLowercaseString());

    if (entry == bridge_->entries().end()) {
      return std::nullopt;
    }
    sync_pb::ProductComparisonSpecifics original = entry->second;
    sync_pb::ProductComparisonSpecifics& specifics = entry->second;
    specifics.set_update_time_unix_epoch_millis(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    specifics.set_name(final_name);
    bridge_->UpdateSpecifics(specifics);
    ProductSpecificationsSet set =
        ProductSpecificationsSet::FromProto(specifics);
    NotifyProductSpecificationsUpdate(
        ProductSpecificationsSet::FromProto(original), set);
    return set;
  }
}

void ProductSpecificationsService::DeleteProductSpecificationsSet(
    const std::string& uuid) {
  if (!is_initialized_) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          commerce::kProductSpecificationsMultiSpecifics)) {
    std::vector<sync_pb::ProductComparisonSpecifics> specifics_to_delete;

    const sync_pb::ProductComparisonSpecifics* top_level = nullptr;
    std::vector<GURL> urls;
    for (auto& entry : bridge_->entries()) {
      if (entry.second.has_product_comparison() &&
          entry.second.uuid() == uuid) {
        specifics_to_delete.push_back(entry.second);
        top_level = &entry.second;
      } else if (entry.second.has_product_comparison_item() &&
                 entry.second.product_comparison_item()
                         .product_comparison_uuid() == uuid) {
        specifics_to_delete.push_back(entry.second);
        urls.emplace_back(entry.second.product_comparison_item().url());
      }
    }
    if (top_level) {
      ProductSpecificationsSet set(
          uuid, top_level->creation_time_unix_epoch_millis(),
          top_level->update_time_unix_epoch_millis(), urls,
          top_level->product_comparison().name());
      NotifyProductSpecificationsRemoval(set);
    }
    bridge_->DeleteSpecifics(specifics_to_delete);
  } else {
    auto it = bridge_->entries().find(uuid);
    if (it == bridge_->entries().end()) {
      return;
    }
    sync_pb::ProductComparisonSpecifics to_remove = it->second;
    bridge_->DeleteSpecifics({to_remove});
    NotifyProductSpecificationsRemoval(
        ProductSpecificationsSet::FromProto(to_remove));
  }
}

void ProductSpecificationsService::AddObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  observers_.AddObserver(observer);
}

void ProductSpecificationsService::RemoveObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProductSpecificationsService::OnInit() {
  is_initialized_ = true;
  for (base::OnceCallback<void(void)>& deferred_operation :
       deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
  MigrateLegacySpecificsIfApplicable();
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
}

void ProductSpecificationsService::OnSpecificsUpdated(
    const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                sync_pb::ProductComparisonSpecifics>>
        before_after_specifics) {
  for (const auto& [before, after] : before_after_specifics) {
    if (!before.has_product_comparison() &&
        !before.has_product_comparison_item() &&
        !after.has_product_comparison() &&
        !after.has_product_comparison_item()) {
      NotifyProductSpecificationsUpdate(
          ProductSpecificationsSet::FromProto(before),
          ProductSpecificationsSet::FromProto(after));
    }
  }
}

void ProductSpecificationsService::OnSpecificsRemoved(
    const std::vector<sync_pb::ProductComparisonSpecifics> removed_specifics) {
  for (auto& specifics : removed_specifics) {
    for (auto& observer : observers_) {
      if (!specifics.has_product_comparison() &&
          !specifics.has_product_comparison_item()) {
        observer.OnProductSpecificationsSetRemoved(
            ProductSpecificationsSet::FromProto(specifics));
      }
    }
  }
}

void ProductSpecificationsService::OnMultiSpecificsChanged(
    const std::vector<sync_pb::ProductComparisonSpecifics> changed_specifics,
    const std::map<std::string, sync_pb::ProductComparisonSpecifics>
        before_entries) {
  std::set<std::string> changed_set_uuids;
  // Determine what ProductSpecificationsSets have changed.
  for (const auto& specific : changed_specifics) {
    // Top level specifics - represents ProductSpecificationsSet as a whole.
    if (specific.has_product_comparison()) {
      changed_set_uuids.insert(specific.uuid());
      // Item level specifics - represents one item in a
      // ProductSpecificationSet and has a reference to the uuid of the
      // top level specific.
    } else if (specific.has_product_comparison_item()) {
      changed_set_uuids.insert(
          specific.product_comparison_item().product_comparison_uuid());
    }
  }
  for (const auto& changed_set_uuid : changed_set_uuids) {
    base::Uuid uuid = base::Uuid::ParseLowercase(changed_set_uuid);
    std::optional<ProductSpecificationsSet> before =
        GetProductSpecificationsSetFromMultiSpecifics(uuid, before_entries);

    std::optional<ProductSpecificationsSet> after =
        GetProductSpecificationsSetFromMultiSpecifics(uuid, bridge_->entries());

    if (!before.has_value() && after.has_value()) {
      NotifyProductSpecificationsAdded(after.value());
    } else if (before.has_value() && after.has_value()) {
      NotifyProductSpecificationsUpdate(before.value(), after.value());
    } else if (before.has_value() && !after.has_value()) {
      // TODO(crbug.com/354010068) ensure we don't inadvertendly
      // delete a set upon sync failure.
      NotifyProductSpecificationsRemoval(before.value());
    }
  }
}

void ProductSpecificationsService::NotifyProductSpecificationsAdded(
    const ProductSpecificationsSet& added_set) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetAdded(added_set);
  }
}

void ProductSpecificationsService::NotifyProductSpecificationsUpdate(
    const ProductSpecificationsSet& before,
    const ProductSpecificationsSet& after) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetUpdate(before, after);

    if (before.name() != after.name()) {
      observer.OnProductSpecificationsSetNameUpdate(before.name(),
                                                    after.name());
    }
  }
}

void ProductSpecificationsService::NotifyProductSpecificationsRemoval(
    const ProductSpecificationsSet& set) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetRemoved(set);
  }
}

void ProductSpecificationsService::MigrateLegacySpecificsIfApplicable() {
  if (kProductSpecsMigrateToMultiSpecifics.Get()) {
    std::vector<sync_pb::ProductComparisonSpecifics> migrate_specifics_to_add;
    for (auto [uuid, specifics] : bridge_->entries()) {
      // It's possible for the legacy format to have no URLs and just a name
      // so detect legacy specifics to have a name and no ProductComparison
      // and no ProductComparisonItem fields.
      if (specifics.has_name() && !specifics.has_product_comparison() &&
          !specifics.has_product_comparison_item()) {
        specifics.mutable_product_comparison()->set_name(specifics.name());
        specifics.clear_name();
        bridge_->UpdateSpecifics(specifics);

        std::vector<GURL> urls;
        for (const sync_pb::ComparisonData& data : specifics.data()) {
          urls.emplace_back(data.url());
        }
        // Title can be left blank in GetUrlInfos, as the migration
        // is of specifics created before we included title.
        bridge_->AddSpecifics(CreateItemLevelSpecifics(
            uuid, GetUrlInfos(urls),
            base::Time::FromMillisecondsSinceUnixEpoch(
                specifics.update_time_unix_epoch_millis())));
        specifics.clear_data();
      }
    }
  }
}

void ProductSpecificationsService::DisableInitializedForTesting() {
  is_initialized_ = false;
}

}  // namespace commerce
