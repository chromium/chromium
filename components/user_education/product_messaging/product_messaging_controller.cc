// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/product_messaging/product_messaging_controller.h"

#include <algorithm>
#include <compare>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/product_messaging/product_messaging_policy.h"

namespace user_education {

// ProductMessagingHandleImpl

ProductMessagingHandleImpl::ProductMessagingHandleImpl(
    ProductMessageKey message_key,
    base::WeakPtr<ProductMessagingController> controller)
    : message_key_(message_key), controller_(controller) {
  CHECK(message_key_);
  CHECK(controller_);
}

ProductMessagingHandleImpl::~ProductMessagingHandleImpl() {
  controller_->ReleaseHandle(message_key_, shown_);
  controller_.reset();
  superseded_subscription_ = {};
  superseded_callback_.Reset();
  message_key_ = ProductMessageKey();
}

void ProductMessagingHandleImpl::SetShown() {
  CHECK(!shown_);
  shown_ = true;
  controller_->OnMessageShown(message_key_);
}

void ProductMessagingHandleImpl::SetSupersededCallback(
    ProductMessageStatusCallback callback) {
  if (callback.is_null()) {
    superseded_subscription_ = {};
    superseded_callback_.Reset();
    return;
  }
  CHECK(!superseded_subscription_);
  CHECK(!superseded_callback_);

  superseded_callback_ = std::move(callback);
  superseded_subscription_ =
      controller_->status_update_callbacks_.Add(base::BindRepeating(
          &ProductMessagingHandleImpl::OnStatusChange, base::Unretained(this)));
}

void ProductMessagingHandleImpl::OnStatusChange(ProductMessageKey key,
                                                ProductMessageStatus status) {
  CHECK(superseded_callback_);
  CHECK(message_key_);
  if (key.type() <= message_key_.type()) {
    return;
  }
  if (status != ProductMessageStatus::kReady &&
      status != ProductMessageStatus::kShowing) {
    return;
  }
  superseded_callback_.Run(key, status);
}

struct ProductMessagingController::QueueData {
  QueueData() = default;
  QueueData(ProductMessageReadyCallback callback_,
            std::optional<base::TimeTicks> expires_at_)
      : callback(std::move(callback_)), expires_at(expires_at_) {}
  QueueData(QueueData&& other) noexcept = default;
  QueueData& operator=(QueueData&& other) noexcept = default;
  ~QueueData() = default;

  ProductMessageReadyCallback callback;
  std::optional<base::TimeTicks> expires_at;

  bool is_expired() const {
    return expires_at && base::TimeTicks::Now() >= expires_at;
  }
};

// ProductMessagingController

ProductMessagingController::ProductMessagingController() = default;
ProductMessagingController::~ProductMessagingController() = default;

void ProductMessagingController::Init(
    UserEducationSessionProvider& session_provider,
    UserEducationStorageService& storage_service,
    std::unique_ptr<ProductMessagingPolicy> policy) {
  CHECK(!storage_service_);
  CHECK(!policy_);
  CHECK(policy);
  storage_service_ = &storage_service;
  if (session_provider.GetNewSessionSinceStartup()) {
    storage_service_->ResetProductMessagingData();
  }
  session_subscription_ =
      session_provider.AddNewSessionCallback(base::BindRepeating(
          &ProductMessagingController::OnNewSession, base::Unretained(this)));
  policy_ = std::move(policy);
}

void ProductMessagingController::QueueMessage(
    ProductMessageKey message_key,
    ProductMessageReadyCallback ready_to_start_callback,
    std::optional<base::TimeDelta> timeout) {
  CHECK(message_key);
  CHECK(!ready_to_start_callback.is_null());
  const std::optional<base::TimeTicks> expiry =
      timeout ? std::make_optional(base::TimeTicks::Now() + *timeout)
              : std::nullopt;

  // Cannot re-queue a notice unless overriding the timer.
  if (active_messages_.contains(message_key)) {
    return;
  } else if (const auto existing = pending_messages_.find(message_key);
             existing != pending_messages_.end()) {
    // If this would not supersede an existing expiry timer, don't queue it.
    if (!existing->second.expires_at ||
        (expiry && expiry <= *existing->second.expires_at)) {
      return;
    }

    // We're overriding the expiry timer, so replace the existing entry.
    pending_messages_.erase(existing);
  }

  // Add the new notice.
  const auto result = pending_messages_.emplace(
      message_key, QueueData(std::move(ready_to_start_callback), expiry));
  CHECK(result.second);
  status_update_callbacks_.Notify(message_key, ProductMessageStatus::kWaiting);
  MaybeShowNextMessage();
}

void ProductMessagingController::UnqueueMessage(ProductMessageKey message_key) {
  pending_messages_.erase(message_key);
}

// Returns the status of `message`.
ProductMessageStatus ProductMessagingController::GetMessageStatus(
    ProductMessageKey message) const {
  if (!message) {
    return ProductMessageStatus::kNone;
  }
  if (const bool* const shown = base::FindOrNull(active_messages_, message)) {
    return *shown ? ProductMessageStatus::kShowing
                  : ProductMessageStatus::kReady;
  }
  if (const auto it = pending_messages_.find(message);
      it != pending_messages_.end()) {
    return it->second.is_expired() ? ProductMessageStatus::kNone
                                   : ProductMessageStatus::kWaiting;
  }
  return ProductMessageStatus::kNone;
}

// Returns queued or showing messages. Can be filtered by priority and by
// status.
base::flat_map<ProductMessageKey, ProductMessageStatus>
ProductMessagingController::GetAllMessages(
    ProductMessageStatusSet statuses_to_retrieve,
    ProductMessageType priority_higher_than) const {
  base::flat_map<ProductMessageKey, ProductMessageStatus> infos;
  for (const auto& [key, shown] : active_messages_) {
    if (key.type() > priority_higher_than) {
      if (shown && statuses_to_retrieve.Has(ProductMessageStatus::kShowing)) {
        infos.emplace(key, ProductMessageStatus::kShowing);
      }
      if (!shown && statuses_to_retrieve.Has(ProductMessageStatus::kReady)) {
        infos.emplace(key, ProductMessageStatus::kReady);
      }
    }
  }
  if (statuses_to_retrieve.Has(ProductMessageStatus::kWaiting)) {
    for (auto& [key, data] : pending_messages_) {
      if (key.type() > priority_higher_than && !data.is_expired()) {
        infos.emplace(key, ProductMessageStatus::kWaiting);
      }
    }
  }
  return infos;
}

base::CallbackListSubscription
ProductMessagingController::AddStatusUpdateCallbackForTesting(
    ProductMessageStatusCallback callback) {
  return status_update_callbacks_.Add(std::move(callback));
}

std::optional<base::TimeDelta>
ProductMessagingController::GetRemainingTimeForTesting(
    ProductMessageKey message_key) const {
  const auto* const entry = base::FindOrNull(pending_messages_, message_key);
  CHECK(entry);
  return entry->expires_at
             ? std::make_optional(*entry->expires_at - base::TimeTicks::Now())
             : std::nullopt;
}

void ProductMessagingController::ReleaseHandle(ProductMessageKey message_key,
                                               bool message_shown) {
  const auto it = active_messages_.find(message_key);
  CHECK(it != active_messages_.end());
  CHECK_EQ(it->second, message_shown);
  if (message_shown) {
    ProductMessagingData data = storage_service_->ReadProductMessagingData();
    const auto insert_result = data.shown_notices.insert(message_key.GetName());
    if (insert_result.second) {
      storage_service_->SaveProductMessagingData(data);
    }
  }
  active_messages_.erase(it);
  MaybeShowNextMessage();
}

void ProductMessagingController::MaybeShowNextMessage() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProductMessagingController::MaybeShowNextMessageImpl,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductMessagingController::PurgeBlockedAndExpiredMessages() {
  // General purpose method to purge pending messages that match a predicate.
  const auto purge_all_that =
      [&](const base::FunctionRef<bool(ProductMessageKey, const QueueData&)>&
              should_purge) {
        std::vector<ProductMessageKey> to_purge;
        for (auto& [key, data] : pending_messages_) {
          if (should_purge(key, data)) {
            to_purge.push_back(key);
          }
        }
        for (auto id : to_purge) {
          pending_messages_.erase(id);
        }
      };

  // Purge timed-out messages.
  purge_all_that([](ProductMessageKey, const QueueData& data) {
    return data.is_expired();
  });

  // Get the IDs of all shown messages.
  ProductMessagingPolicy::Ids shown_ids;
  for (const auto& name :
       storage_service_->ReadProductMessagingData().shown_notices) {
    std::string actual_name = name;
    actual_name += internal::kProductMessageUniqueIdSuffix;
    const auto id =
        internal::ProductMessageUniqueId::FromName(actual_name.c_str());
    shown_ids.insert(id);
  }

  // Purge all blocked messages.
  purge_all_that([&](ProductMessageKey key, const QueueData&) {
    return policy_->BlockedByAnyOf(key, shown_ids, /*include_self=*/true);
  });
}

void ProductMessagingController::MaybeShowNextMessageImpl() {
  PurgeBlockedAndExpiredMessages();
  if (pending_messages_.empty()) {
    return;
  }

  // Build some lookups for faster access.
  ProductMessagingPolicy::Ids ids;
  ProductMessageTypeSet queued_types;
  ProductMessageTypeSet active_types;
  for (const auto& [key, data] : pending_messages_) {
    ids.insert(key.id());
    queued_types.Put(key.type());
  }
  for (const auto& [key, data] : active_messages_) {
    ids.insert(key.id());
    active_types.Put(key.type());
  }

  // Find a notice that is not waiting for any other notices to show.
  ProductMessageKey to_show;
  for (const auto& [key, data] : pending_messages_) {
    // Ensure that the message is not temporarily or permanently blocked.
    if (policy_->BlockedByAnyOf(key, ids, /*include_self=*/false) ||
        policy_->MustShowAfterAnyOf(key, ids)) {
      continue;
    }

    // Ensure that the message is not superseded by any queued message.
    if (std::ranges::any_of(queued_types, [this, key](ProductMessageType type) {
          return policy_->GetRelationship(key.type(), type) ==
                 ProductMessagingPolicy::TypeRelationship::kSupersededBy;
        })) {
      continue;
    }

    // Ensure that the message is not superseded or excluded by any active
    // message.
    if (std::ranges::any_of(active_types, [this, key](ProductMessageType type) {
          const auto relationship = policy_->GetRelationship(key.type(), type);
          return relationship ==
                     ProductMessagingPolicy::TypeRelationship::kSupersededBy ||
                 relationship ==
                     ProductMessagingPolicy::TypeRelationship::kEquivalentTo;
        })) {
      continue;
    }

    // Choose this message.
    to_show = key;
    break;
  }

  if (!to_show) {
    CHECK(!active_messages_.empty())
        << "Circular dependency in required notifications:" << DumpData();
    return;
  }

  // Fire the next notice.
  ProductMessageReadyCallback cb =
      std::move(pending_messages_[to_show].callback);
  pending_messages_.erase(to_show);
  active_messages_.emplace(to_show, false);
  std::move(cb).Run(base::WrapUnique(
      new ProductMessagingHandleImpl(to_show, weak_ptr_factory_.GetWeakPtr())));
  status_update_callbacks_.Notify(to_show, ProductMessageStatus::kReady);

  // Since multiple messages can show at the same time (if they are independent)
  // we might need to check again.
  if (!pending_messages_.empty()) {
    MaybeShowNextMessage();
  }
}

void ProductMessagingController::OnNewSession() {
  storage_service_->ResetProductMessagingData();
}

void ProductMessagingController::OnMessageShown(ProductMessageKey message_key) {
  const auto it = active_messages_.find(message_key);
  CHECK(it != active_messages_.end());
  if (it->second) {
    return;
  }
  it->second = true;
  status_update_callbacks_.Notify(message_key, ProductMessageStatus::kShowing);
}

std::string ProductMessagingController::DumpData() const {
  std::ostringstream oss;
  for (const auto& [key, data] : pending_messages_) {
    oss << "\n{ key: " << key.ToString();
    if (data.expires_at) {
      oss << " expires_at: " << *data.expires_at;
    } else {
      oss << " (does not expire)";
    }
    oss << " }";
  }
  return oss.str();
}

}  // namespace user_education
