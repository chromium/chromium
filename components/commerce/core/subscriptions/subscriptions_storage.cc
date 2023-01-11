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

SubscriptionsStorage::SubscriptionsStorage(
    SessionProtoStorage<CommerceSubscriptionProto>* subscription_proto_db)
    : proto_db_(subscription_proto_db), weak_ptr_factory_(this) {}
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
  LoadAllSubscriptionsForType(
      type, base::BindOnce(&SubscriptionsStorage::PerformUpdateStorage,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(remote_subscriptions)));
}

void SubscriptionsStorage::DeleteAll() {
  proto_db_->DeleteAllContent(base::BindOnce([](bool succeeded) {
    if (!succeeded)
      VLOG(1) << "Fail to delete all subscriptions";
  }));
}

std::string SubscriptionsStorage::GetSubscriptionKey(
    const CommerceSubscription& subscription) {
  return SubscriptionTypeToString(subscription.type) + "_" +
         SubscriptionIdTypeToString(subscription.id_type) + "_" +
         subscription.id;
}

void SubscriptionsStorage::SaveSubscription(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  // Get proto types from the object.
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

  // TODO(crbug.com/1348024): Record metrics for failed parse.
  if (!type_parse_succeeded || !id_type_parse_succeeded ||
      !management_type_parse_succeeded) {
    VLOG(1) << "Fail to get proto type";
    std::move(callback).Run(false);
    return;
  }

  const std::string& key = GetSubscriptionKey(subscription);
  CommerceSubscriptionProto proto;
  proto.set_key(key);
  proto.set_tracking_id(subscription.id);
  proto.set_subscription_type(subscription_type);
  proto.set_tracking_id_type(tracking_id_type);
  proto.set_management_type(management_type);
  proto.set_timestamp(subscription.timestamp);

  proto_db_->InsertContent(key, proto, std::move(callback));
}

void SubscriptionsStorage::DeleteSubscription(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  proto_db_->DeleteOneEntry(GetSubscriptionKey(subscription),
                            std::move(callback));
}

void SubscriptionsStorage::LoadAllSubscriptionsForType(
    SubscriptionType type,
    GetLocalSubscriptionsCallback callback) {
  proto_db_->LoadContentWithPrefix(
      SubscriptionTypeToString(type),
      base::BindOnce(
          [](base::WeakPtr<SubscriptionsStorage> storage,
             GetLocalSubscriptionsCallback callback, bool succeeded,
             CommerceSubscriptions data) {
            auto subscriptions =
                std::make_unique<std::vector<CommerceSubscription>>();
            if (!succeeded) {
              VLOG(1) << "Fail to load all subscriptions";
              std::move(callback).Run(std::move(subscriptions));
              return;
            }
            for (SessionProtoStorage<CommerceSubscriptionProto>::KeyAndValue&
                     kv : data) {
              subscriptions->push_back(storage->GetSubscriptionFromProto(kv));
            }
            std::move(callback).Run(std::move(subscriptions));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    std::string key = GetSubscriptionKey(subscription);
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
    StorageOperationCallback callback,
    std::unique_ptr<std::vector<CommerceSubscription>> remote_subscriptions,
    std::unique_ptr<std::vector<CommerceSubscription>> local_subscriptions) {
  auto remote_map = SubscriptionsListToMap(std::move(remote_subscriptions));
  auto local_map = SubscriptionsListToMap(std::move(local_subscriptions));
  bool all_succeeded = true;
  for (auto& kv : local_map) {
    if (remote_map.find(kv.first) == remote_map.end()) {
      DeleteSubscription(std::move(kv.second),
                         base::BindOnce(
                             [](bool* all_succeeded, bool succeeded) {
                               *all_succeeded = (*all_succeeded) && succeeded;
                             },
                             &all_succeeded));
    }
  }
  for (auto& kv : remote_map) {
    if (local_map.find(kv.first) == local_map.end()) {
      SaveSubscription(std::move(kv.second),
                       base::BindOnce(
                           [](bool* all_succeeded, bool succeeded) {
                             *all_succeeded = (*all_succeeded) && succeeded;
                           },
                           &all_succeeded));
    }
  }
  std::move(callback).Run(all_succeeded
                              ? SubscriptionsRequestStatus::kSuccess
                              : SubscriptionsRequestStatus::kStorageError);
}

void SubscriptionsStorage::IsSubscribed(
    CommerceSubscription subscription,
    base::OnceCallback<void(bool)> callback) {
  proto_db_->LoadOneEntry(
      GetSubscriptionKey(subscription),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, bool succeeded,
             CommerceSubscriptions data) {
            std::move(callback).Run(succeeded && data.size() > 0);
          },
          std::move(callback)));
}

}  // namespace commerce
