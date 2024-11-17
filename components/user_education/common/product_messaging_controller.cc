// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/product_messaging_controller.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

namespace internal {
DEFINE_REQUIRED_NOTICE_IDENTIFIER(kShowAfterAllNotices);
}

// RequiredNoticePriorityHandle

RequiredNoticePriorityHandle::RequiredNoticePriorityHandle() = default;

RequiredNoticePriorityHandle::RequiredNoticePriorityHandle(
    RequiredNoticeId notice_id,
    base::WeakPtr<ProductMessagingController> controller)
    : notice_id_(notice_id), controller_(controller) {}

RequiredNoticePriorityHandle::RequiredNoticePriorityHandle(
    RequiredNoticePriorityHandle&& other) noexcept
    : notice_id_(std::exchange(other.notice_id_, RequiredNoticeId())),
      controller_(std::move(other.controller_)) {}

RequiredNoticePriorityHandle& RequiredNoticePriorityHandle::operator=(
    RequiredNoticePriorityHandle&& other) noexcept {
  if (this != &other) {
    notice_id_ = std::exchange(other.notice_id_, RequiredNoticeId());
    controller_ = std::move(other.controller_);
  }
  return *this;
}

RequiredNoticePriorityHandle::~RequiredNoticePriorityHandle() {
  Release();
}

RequiredNoticePriorityHandle::operator bool() const {
  return notice_id_ && controller_;
}

bool RequiredNoticePriorityHandle::operator!() const {
  return !static_cast<bool>(*this);
}

void RequiredNoticePriorityHandle::SetShown() {
  CHECK(static_cast<bool>(this));
  shown_ = true;
}

void RequiredNoticePriorityHandle::Release() {
  if (!*this) {
    return;
  }

  controller_->ReleaseHandle(notice_id_, shown_);
  controller_.reset();
  notice_id_ = RequiredNoticeId();
}

// ProductMessagingController::RequiredNoticeData

struct ProductMessagingController::RequiredNoticeData {
  RequiredNoticeData() = default;
  RequiredNoticeData(RequiredNoticeData&&) = default;
  RequiredNoticeData& operator=(RequiredNoticeData&&) = default;
  RequiredNoticeData(RequiredNoticeShowCallback callback_,
                     std::vector<RequiredNoticeId> show_after_,
                     std::vector<RequiredNoticeId> blocked_by_)
      : callback(std::move(callback_)),
        show_after(std::move(show_after_)),
        blocked_by(std::move(blocked_by_)) {}
  ~RequiredNoticeData() = default;

  RequiredNoticeShowCallback callback;
  std::vector<RequiredNoticeId> show_after;
  std::vector<RequiredNoticeId> blocked_by;
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

bool ProductMessagingController::IsNoticeQueued(
    RequiredNoticeId notice_id) const {
  return base::Contains(pending_notices_, notice_id);
}

void ProductMessagingController::QueueRequiredNotice(
    RequiredNoticeId notice_id,
    RequiredNoticeShowCallback ready_to_start_callback,
    std::initializer_list<RequiredNoticeId> always_show_after,
    std::initializer_list<RequiredNoticeId> blocked_by) {
  CHECK(notice_id);
  CHECK(!ready_to_start_callback.is_null());
  CHECK(!base::Contains(blocked_by, internal::kShowAfterAllNotices));

  // Cannot re-queue the current notice.
  if (current_notice_ == notice_id) {
    return;
  }

  const auto result = pending_notices_.emplace(
      notice_id,
      RequiredNoticeData(std::move(ready_to_start_callback),
                         std::move(always_show_after), std::move(blocked_by)));
  CHECK(result.second) << "Duplicate required notice ID: " << notice_id;
  MaybeShowNextRequiredNotice();
}

void ProductMessagingController::UnqueueRequiredNotice(
    RequiredNoticeId notice_id) {
  pending_notices_.erase(notice_id);
}

void ProductMessagingController::ReleaseHandle(RequiredNoticeId notice_id,
                                               bool notice_shown) {
  CHECK_EQ(current_notice_, notice_id);
  if (notice_shown) {
    ProductMessagingData data = storage_service_->ReadProductMessagingData();
    const auto insert_result = data.shown_notices.insert(notice_id.GetName());
    if (insert_result.second) {
      storage_service_->SaveProductMessagingData(data);
    }
  }
  current_notice_ = RequiredNoticeId();
  MaybeShowNextRequiredNotice();
}

void ProductMessagingController::MaybeShowNextRequiredNotice() {
  if (!ready_to_show()) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ProductMessagingController::MaybeShowNextRequiredNoticeImpl,
          weak_ptr_factory_.GetWeakPtr()));
}

void ProductMessagingController::PurgeBlockedNotices() {
  ProductMessagingData stored_data =
      storage_service_->ReadProductMessagingData();
  std::vector<RequiredNoticeId> to_purge;
  for (const auto& [id, data] : pending_notices_) {
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
    pending_notices_.erase(id);
  }
}

void ProductMessagingController::MaybeShowNextRequiredNoticeImpl() {
  if (!ready_to_show()) {
    return;
  }

  PurgeBlockedNotices();
  if (pending_notices_.empty()) {
    return;
  }

  // Find a notice that is not waiting for any other notices to show.
  RequiredNoticeId to_show;
  for (const auto& [id, data] : pending_notices_) {
    bool excluded = false;
    bool show_after_all = false;
    for (auto after : data.show_after) {
      if (after == internal::kShowAfterAllNotices) {
        show_after_all = true;
      } else if (pending_notices_.contains(after)) {
        excluded = true;
        break;
      }
    }
    for (auto blocker : data.blocked_by) {
      if (pending_notices_.contains(blocker)) {
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
  RequiredNoticeShowCallback cb = std::move(pending_notices_[to_show].callback);
  pending_notices_.erase(to_show);
  current_notice_ = to_show;
  std::move(cb).Run(
      RequiredNoticePriorityHandle(to_show, weak_ptr_factory_.GetWeakPtr()));
}

void ProductMessagingController::OnNewSession() {
  storage_service_->ResetProductMessagingData();
}

std::string ProductMessagingController::DumpData() const {
  std::ostringstream oss;
  for (const auto& [id, data] : pending_notices_) {
    oss << "\n{ id: " << id << " show_after: { ";
    for (const auto& after : data.show_after) {
      oss << after << ", ";
    }
    oss << "} blocked_by: { ";
    for (const auto& blocker : data.blocked_by) {
      oss << blocker << ", ";
    }
    oss << "} }";
  }
  return oss.str();
}

}  // namespace user_education
