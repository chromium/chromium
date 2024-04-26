// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

std::optional<CommerceSubscriptionProto> GetCommerceSubscriptionProto(
    const CommerceSubscription& subscription) {
  SubscriptionTypeProto subscription_type = commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionType_TYPE_UNSPECIFIED;
  bool type_parse_succeeded = commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionType_Parse(
          SubscriptionTypeToString(subscription.type), &subscription_type);

  TrackingIdTypeProto tracking_id_type = commerce_subscription_db::
      CommerceSubscriptionContentProto_TrackingIdType_IDENTIFIER_TYPE_UNSPECIFIED;
  bool id_type_parse_succeeded = commerce_subscription_db::
      CommerceSubscriptionContentProto_TrackingIdType_Parse(
          SubscriptionIdTypeToString(subscription.id_type), &tracking_id_type);

  SubscriptionManagementTypeProto management_type = commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionManagementType_MANAGE_TYPE_UNSPECIFIED;
  bool management_type_parse_succeeded = commerce_subscription_db::
      CommerceSubscriptionContentProto_SubscriptionManagementType_Parse(
          SubscriptionManagementTypeToString(subscription.management_type),
          &management_type);

  // TODO(crbug.com/40233201): Record metrics for failed parse.
  if (!type_parse_succeeded || !id_type_parse_succeeded ||
      !management_type_parse_succeeded) {
    VLOG(1) << "Fail to get proto type";
    return std::nullopt;
  }

  const std::string& key = GetStorageKeyForSubscription(subscription);
  CommerceSubscriptionProto proto;
  proto.set_key(key);
  proto.set_tracking_id(subscription.id);
  proto.set_subscription_type(subscription_type);
  proto.set_tracking_id_type(tracking_id_type);
  proto.set_management_type(management_type);
  proto.set_timestamp(subscription.timestamp);
  return proto;
}

SubscriptionsStorage::SubscriptionsStorage(
    SessionProtoStorage<CommerceSubscriptionProto>* subscription_proto_db)
    : proto_db_(subscription_proto_db) {
  // Populate the cache from local storage.
  LoadAllSubscriptions(base::BindOnce(
      [](base::WeakPtr<SubscriptionsStorage> storage,
         std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
        if (!storage) {
          return;
        }
        for (auto& sub : *subscriptions) {
          storage->subscriptions_cache_.insert(
              GetStorageKeyForSubscription(sub));
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

SubscriptionsStorage::SubscriptionsStorage() = default;
SubscriptionsStorage::~SubscriptionsStorage() = default;

void SubscriptionsStorage::GetUniqueNonExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    GetLocalSubscriptionsCallback callback) {
  CHECK(subscriptions && subscriptions->size() > 0);
  SubscriptionType type = (*subscriptions)[0].type;
  LoadAllSubscriptionsForType(
      type,
      base::BindOnce(&SubscriptionsStorage::PerformGetNonExistingSubscriptions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(subscriptions),
                     std::move(callback)));
}

void SubscriptionsStorage::GetUniqueExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    GetLocalSubscriptionsCallback callback) {
  CHECK(subscriptions && subscriptions->size() > 0);
  SubscriptionType type = (*subscriptions)[0].type;
  LoadAllSubscriptionsForType(
      type,
      base::BindOnce(&SubscriptionsStorage::PerformGetExistingSubscriptions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(subscriptions),
                     std::move(callback)));
}

void SubscriptionsStorage::UpdateStorage(
    SubscriptionType type,
    StorageOperationCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {
  UpdateStorageAndNotifyModifiedSubscriptions(
      type,
      base::BindOnce(
          [](StorageOperationCallback callback,
             SubscriptionsRequestStatus status,
             std::vector<CommerceSubscription> added_subs,
             std::vector<CommerceSubscription> removed_subs) {
            std::move(callback).Run(status);
          },
          std::move(callback)),
      std::move(remote_subscriptions));
}

void SubscriptionsStorage::UpdateStorageAndNotifyModifiedSubscriptions(
    SubscriptionType type,
    StorageUpdateCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions) {
  LoadAllSubscriptionsForType(
      type, base::BindOnce(&SubscriptionsStorage::PerformUpdateStorage,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(remote_subscriptions)));
}

void SubscriptionsStorage::DeleteAll() {
  proto_db_->DeleteAllContent(base::BindOnce(
      [](base::WeakPtr<SubscriptionsStorage> storage, bool succeeded) {
        if (!succeeded) {
          VLOG(1) << "Fail to delete all subscriptions";
        }
        // Just clear the cache regardless of whether the storage is
        // successfully cleared or not, as it is possible that only part of the
        // data got deleted.
        if (storage) {
          storage->subscriptions_cache_.clear();
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void SubscriptionsStorage::SaveSubscription(
    const CommerceSubscription& subscription,
    base::OnceCallback<void(bool)> callback) {
  auto proto = GetCommerceSubscriptionProto(subscription);

  // TODO(crbug.com/40233201): Record metrics for failed parse.
  if (!proto.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  proto_db_->InsertContent(GetStorageKeyForSubscription(subscription),
                           proto.value(), std::move(callback));
}

void SubscriptionsStorage::DeleteSubscription(
    const CommerceSubscription& subscription,
    base::OnceCallback<void(bool)> callback) {
  proto_db_->DeleteOneEntry(GetStorageKeyForSubscription(subscription),
                            std::move(callback));
}

void SubscriptionsStorage::LoadAllSubscriptionsForType(
    SubscriptionType type,
    GetLocalSubscriptionsCallback callback) {
  proto_db_->LoadContentWithPrefix(
      SubscriptionTypeToString(type),
      base::BindOnce(&SubscriptionsStorage::HandleLoadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsStorage::LoadAllSubscriptions(
    GetLocalSubscriptionsCallback callback) {
  proto_db_->LoadAllEntries(
      base::BindOnce(&SubscriptionsStorage::HandleLoadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsStorage::HandleLoadCompleted(
    GetLocalSubscriptionsCallback callback,
    bool succeeded,
    CommerceSubscriptions data) {
  auto subscriptions = std::make_unique<std::vector<CommerceSubscription>>();
  if (!succeeded) {
    VLOG(1) << "Fail to load all subscriptions";
    std::move(callback).Run(std::move(subscriptions));
    return;
  }
  for (SessionProtoStorage<CommerceSubscriptionProto>::KeyAndValue& kv : data) {
    subscriptions->push_back(GetSubscriptionFromProto(kv));
  }
  std::move(callback).Run(std::move(subscriptions));
}

CommerceSubscription SubscriptionsStorage::GetSubscriptionFromProto(
    const SessionProtoStorage<CommerceSubscriptionProto>::KeyAndValue& kv) {
  CommerceSubscriptionProto proto = std::move(kv.second);
  return CommerceSubscription(
      SubscriptionType(proto.subscription_type()),
      IdentifierType(proto.tracking_id_type()), proto.tracking_id(),
      ManagementType(proto.management_type()), proto.timestamp());
}

std::unordered_map<std::string, CommerceSubscription>
SubscriptionsStorage::SubscriptionsListToMap(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
  std::unordered_map<std::string, CommerceSubscription> map;
  for (auto& subscription : *subscriptions) {
    std::string key = GetStorageKeyForSubscription(subscription);
    map.insert(std::make_pair(key, std::move(subscription)));
  }
  return map;
}

void SubscriptionsStorage::PerformGetNonExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> incoming_subscriptions,
    GetLocalSubscriptionsCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions) {
  auto incoming_map = SubscriptionsListToMap(std::move(incoming_subscriptions));
  auto local_map = SubscriptionsListToMap(std::move(local_subscriptions));
  auto subscriptions = std::make_unique<std::vector<CommerceSubscription>>();
  for (auto& kv : incoming_map) {
    if (local_map.find(kv.first) == local_map.end()) {
      subscriptions->push_back(std::move(kv.second));
    }
  }
  std::move(callback).Run(std::move(subscriptions));
}

void SubscriptionsStorage::PerformGetExistingSubscriptions(
    std::unique_ptr<std::vector<CommerceSubscription>> incoming_subscriptions,
    GetLocalSubscriptionsCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions) {
  auto incoming_map = SubscriptionsListToMap(std::move(incoming_subscriptions));
  auto local_map = SubscriptionsListToMap(std::move(local_subscriptions));
  auto subscriptions = std::make_unique<std::vector<CommerceSubscription>>();
  for (auto& kv : incoming_map) {
    auto it = local_map.find(kv.first);
    if (it != local_map.end()) {
      // Push local subscription instead of the incoming one to make sure it has
      // a valid timestamp.
      subscriptions->push_back(std::move(it->second));
      local_map.erase(it);
    }
  }
  std::move(callback).Run(std::move(subscriptions));
}

void SubscriptionsStorage::PerformUpdateStorage(
    StorageUpdateCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions,
    std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions) {
  auto remote_map = SubscriptionsListToMap(std::move(remote_subscriptions));
  auto local_map = SubscriptionsListToMap(std::move(local_subscriptions));
  std::vector<CommerceSubscription> added_subscriptions;
  std::vector<CommerceSubscription> removed_subscriptions;
  auto updated_entries = std::make_unique<
      std::vector<std::pair<std::string, CommerceSubscriptionProto>>>();
  auto removed_keys = std::make_unique<std::vector<std::string>>();

  for (auto& kv : local_map) {
    if (remote_map.find(kv.first) == remote_map.end()) {
      removed_keys->emplace_back(GetStorageKeyForSubscription(kv.second));
      removed_subscriptions.emplace_back(kv.second);
    }
  }
  for (auto& kv : remote_map) {
    auto local_it = local_map.find(kv.first);
    // If there is one subscription in local cache with the same key but
    // different timestamp, we need to replace it with the server-side one since
    // we use the timestamp as the identifier when removing subscription from
    // server. This will ensure the unsubscribe operation works correctly when a
    // user operates on multiple devices.
    if (local_it != local_map.end()) {
      if (local_it->second.timestamp == kv.second.timestamp) {
        continue;
      }
    }
    auto subscription_proto = GetCommerceSubscriptionProto(kv.second);
    if (!subscription_proto.has_value()) {
      continue;
    }
    added_subscriptions.emplace_back(kv.second);
    updated_entries->emplace_back(GetStorageKeyForSubscription(kv.second),
                                  subscription_proto.value());
  }

  proto_db_->UpdateEntries(
      std::move(updated_entries), std::move(removed_keys),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsStorage> storage,
             std::vector<CommerceSubscription> added_subs,
             std::vector<CommerceSubscription> removed_subs,
             StorageUpdateCallback update_callback, bool succeeded) {
            if (storage && succeeded) {
              for (auto& subscription : added_subs) {
                storage->subscriptions_cache_.insert(
                    GetStorageKeyForSubscription(subscription));
              }
              for (auto& subscription : removed_subs) {
                storage->subscriptions_cache_.erase(
                    GetStorageKeyForSubscription(subscription));
              }
            }
            std::move(update_callback)
                .Run(succeeded ? SubscriptionsRequestStatus::kSuccess
                               : SubscriptionsRequestStatus::kStorageError,
                     std::move(added_subs), std::move(removed_subs));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(added_subscriptions),
          std::move(removed_subscriptions), std::move(callback)));
}

void SubscriptionsStorage::IsSubscribed(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  std::string key = GetStorageKeyForSubscription(subscription);
  proto_db_->LoadOneEntry(
      key, base::BindOnce(
               [](base::WeakPtr<SubscriptionsStorage> storage, std::string key,
                  base::OnceCallback<void(bool)> callback, bool succeeded,
                  CommerceSubscriptions data) {
                 if (storage && succeeded) {
                   if (data.size() > 0) {
                     storage->subscriptions_cache_.insert(key);
                   } else {
                     storage->subscriptions_cache_.erase(key);
                   }
                 }
                 std::move(callback).Run(succeeded && data.size() > 0);
               },
               weak_ptr_factory_.GetWeakPtr(), key, std::move(callback)));
}

bool SubscriptionsStorage::IsSubscribedFromCache(
    const CommerceSubscription& subscription) {
  return subscriptions_cache_.contains(
      GetStorageKeyForSubscription(subscription));
}

}  // namespace commerce
