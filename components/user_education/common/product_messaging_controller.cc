// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/product_messaging_controller.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

bool ProductMessageKey::operator<(ProductMessageKey other) const {
  DCHECK(id_ != other.id_ || type_ == other.type_)
      << "Found two product message keys with same id " << id_
      << " but different types.";
  return id_ < other.id_;
}

std::string ProductMessageKey::GetName() const {
  static constexpr std::string_view kSuffix = "UniqueId";
  std::string temp = id_.GetName();
  if (!temp.empty()) {
    CHECK(temp.ends_with(kSuffix));
    temp = temp.substr(0, temp.length() - kSuffix.length());
  }
  return temp;
}

std::string ProductMessageKey::ToString() const {
  static constexpr std::array<
      std::string_view, static_cast<size_t>(ProductMessageType::kMaxValue) + 1U>
      kTypeNames{"[none]", "LowPriorityIph", "HighPriorityIph",
                 "LegalOrComplianceNotice"};
  std::ostringstream oss;
  oss << "ProductMessageKey{ type: "
      << kTypeNames.at(static_cast<size_t>(type_)) << " id: " << id_.GetName()
      << " }";
  return oss.str();
}

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
  if (status != ProductMessageStatus::kEligible &&
      status != ProductMessageStatus::kShowing) {
    return;
  }
  superseded_callback_.Run(key, status);
}

// ProductMessagingController::ProductMessageData

struct ProductMessagingController::ProductMessageData {
  ProductMessageData() = default;
  ProductMessageData(ProductMessageData&&) = default;
  ProductMessageData& operator=(ProductMessageData&&) = default;
  ProductMessageData(ProductMessageReadyCallback callback_,
                     std::vector<ProductMessageKey> show_after_,
                     std::vector<ProductMessageKey> blocked_by_)
      : callback(std::move(callback_)),
        show_after(std::move(show_after_)),
        blocked_by(std::move(blocked_by_)) {}
  ~ProductMessageData() = default;

  ProductMessageReadyCallback callback;
  std::vector<ProductMessageKey> show_after;
  std::vector<ProductMessageKey> blocked_by;
};

// ProductMessagingController

ProductMessagingController::ProductMessagingController() = default;
ProductMessagingController::~ProductMessagingController() = default;

void ProductMessagingController::Init(
    UserEducationSessionProvider& session_provider,
    UserEducationStorageService& storage_service) {
  storage_service_ = &storage_service;
  if (session_provider.GetNewSessionSinceStartup()) {
    storage_service_->ResetProductMessagingData();
  }
  session_subscription_ =
      session_provider.AddNewSessionCallback(base::BindRepeating(
          &ProductMessagingController::OnNewSession, base::Unretained(this)));
}

bool ProductMessagingController::IsMessageQueued(
    ProductMessageKey message_key) const {
  return pending_messages_.contains(message_key);
}

void ProductMessagingController::QueueMessage(
    ProductMessageKey message_key,
    ProductMessageReadyCallback ready_to_start_callback,
    std::initializer_list<ProductMessageKey> always_show_after,
    std::initializer_list<ProductMessageKey> blocked_by) {
  CHECK(message_key);
  CHECK(!ready_to_start_callback.is_null());

  // Cannot re-queue the current notice.
  if (current_message_ == message_key) {
    return;
  }

  const auto result = pending_messages_.emplace(
      message_key,
      ProductMessageData(std::move(ready_to_start_callback),
                         std::move(always_show_after), std::move(blocked_by)));
  CHECK(result.second) << "Duplicate message ID: " << message_key.ToString();
  status_update_callbacks_.Notify(message_key, ProductMessageStatus::kQueued);
  MaybeShowNextMessage();
}

void ProductMessagingController::UnqueueMessage(ProductMessageKey message_key) {
  pending_messages_.erase(message_key);
}

// Returns the status of `message`.
ProductMessageStatus ProductMessagingController::GetProductMessageStatus(
    ProductMessageKey message) const {
  if (!message) {
    return ProductMessageStatus::kNone;
  }
  if (message == current_message_) {
    return current_message_shown_ ? ProductMessageStatus::kShowing
                                  : ProductMessageStatus::kEligible;
  }
  if (IsMessageQueued(message)) {
    return ProductMessageStatus::kQueued;
  }
  return ProductMessageStatus::kNone;
}

// Returns queued or showing messages. Can be filtered by priority and by
// status.
base::flat_map<ProductMessageKey, ProductMessageStatus>
ProductMessagingController::GetAllMessages(
    std::initializer_list<ProductMessageStatus> statuses_to_retrieve,
    ProductMessageType priority_higher_than) const {
  const base::flat_set<ProductMessageStatus> filter_statuses(
      statuses_to_retrieve);
  base::flat_map<ProductMessageKey, ProductMessageStatus> infos;
  if (current_message_ && current_message_.type() > priority_higher_than) {
    if (current_message_shown_ &&
        filter_statuses.contains(ProductMessageStatus::kShowing)) {
      infos.emplace(current_message_, ProductMessageStatus::kShowing);
    }
    if (!current_message_shown_ &&
        filter_statuses.contains(ProductMessageStatus::kEligible)) {
      infos.emplace(current_message_, ProductMessageStatus::kEligible);
    }
  }
  if (filter_statuses.contains(ProductMessageStatus::kQueued)) {
    for (auto& [key, data] : pending_messages_) {
      if (key.type() > priority_higher_than) {
        infos.emplace(key, ProductMessageStatus::kQueued);
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

bool ProductMessagingController::HasPendingMessagesForTesting() const {
  return current_message_ || !pending_messages_.empty();
}

void ProductMessagingController::ReleaseHandle(ProductMessageKey message_key,
                                               bool message_shown) {
  CHECK_EQ(current_message_, message_key);
  if (message_shown) {
    ProductMessagingData data = storage_service_->ReadProductMessagingData();
    const auto insert_result = data.shown_notices.insert(message_key.GetName());
    if (insert_result.second) {
      storage_service_->SaveProductMessagingData(data);
    }
  }
  current_message_ = ProductMessageKey();
  current_message_shown_ = false;
  MaybeShowNextMessage();
}

void ProductMessagingController::MaybeShowNextMessage() {
  if (!ready_to_show()) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProductMessagingController::MaybeShowNextMessageImpl,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductMessagingController::PurgeBlockedMessages() {
  ProductMessagingData stored_data =
      storage_service_->ReadProductMessagingData();
  std::vector<ProductMessageKey> to_purge;
  for (const auto& [id, data] : pending_messages_) {
    if (stored_data.shown_notices.contains(id.GetName())) {
      to_purge.push_back(id);
      continue;
    }
    for (auto blocker : data.blocked_by) {
      if (stored_data.shown_notices.contains(blocker.GetName())) {
        to_purge.push_back(id);
        break;
      }
    }
  }
  for (auto id : to_purge) {
    pending_messages_.erase(id);
  }
}

void ProductMessagingController::MaybeShowNextMessageImpl() {
  if (!ready_to_show()) {
    return;
  }

  PurgeBlockedMessages();
  if (pending_messages_.empty()) {
    return;
  }

  // Find a notice that is not waiting for any other notices to show.
  ProductMessageKey to_show;
  for (const auto& [id, data] : pending_messages_) {
    bool excluded = false;
    bool show_after_all = false;
    for (auto after : data.show_after) {
      if (after.type() <= ProductMessageType::kLowPriorityIph) {
        show_after_all = true;
      } else if (pending_messages_.contains(after)) {
        excluded = true;
        break;
      }
    }
    for (auto blocker : data.blocked_by) {
      if (pending_messages_.contains(blocker)) {
        excluded = true;
        break;
      }
    }
    if (!excluded) {
      if (!show_after_all) {
        to_show = id;
        break;
      } else if (!to_show) {
        to_show = id;
      }
    }
  }

  if (!to_show) {
    NOTREACHED() << "Circular dependency in required notifications:"
                 << DumpData();
  }

  // Fire the next notice.
  ProductMessageReadyCallback cb =
      std::move(pending_messages_[to_show].callback);
  pending_messages_.erase(to_show);
  current_message_ = to_show;
  current_message_shown_ = false;
  std::move(cb).Run(base::WrapUnique(
      new ProductMessagingHandleImpl(to_show, weak_ptr_factory_.GetWeakPtr())));
  status_update_callbacks_.Notify(to_show, ProductMessageStatus::kEligible);
}

void ProductMessagingController::OnNewSession() {
  storage_service_->ResetProductMessagingData();
}

void ProductMessagingController::OnMessageShown(ProductMessageKey message_key) {
  if (message_key == current_message_) {
    current_message_shown_ = true;
    status_update_callbacks_.Notify(message_key,
                                    ProductMessageStatus::kShowing);
  }
}

std::string ProductMessagingController::DumpData() const {
  std::ostringstream oss;
  for (const auto& [key, data] : pending_messages_) {
    oss << "\n{ key: " << key.ToString() << " show_after: { ";
    for (const auto& after : data.show_after) {
      oss << after.ToString() << ", ";
    }
    oss << "} blocked_by: { ";
    for (const auto& blocker : data.blocked_by) {
      oss << blocker.ToString() << ", ";
    }
    oss << "} }";
  }
  return oss.str();
}

}  // namespace user_education
