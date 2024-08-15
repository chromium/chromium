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

void RequiredNoticePriorityHandle::Release() {
  if (!*this) {
    return;
  }

  controller_->ReleaseHandle(notice_id_);
  controller_.reset();
  notice_id_ = RequiredNoticeId();
}

// ProductMessagingController::RequiredNoticeData

struct ProductMessagingController::RequiredNoticeData {
  RequiredNoticeData() = default;
  RequiredNoticeData(RequiredNoticeData&&) = default;
  RequiredNoticeData& operator=(RequiredNoticeData&&) = default;
  RequiredNoticeData(RequiredNoticeShowCallback callback_,
                     std::vector<RequiredNoticeId> show_after_)
      : callback(std::move(callback_)), show_after(std::move(show_after_)) {}
  ~RequiredNoticeData() = default;

  RequiredNoticeShowCallback callback;
  std::vector<RequiredNoticeId> show_after;
};

// ProductMessagingController

ProductMessagingController::ProductMessagingController() = default;
ProductMessagingController::~ProductMessagingController() = default;

bool ProductMessagingController::IsNoticeQueued(
    RequiredNoticeId notice_id) const {
  return base::Contains(data_, notice_id);
}

void ProductMessagingController::QueueRequiredNotice(
    RequiredNoticeId notice_id,
    RequiredNoticeShowCallback ready_to_start_callback,
    std::initializer_list<RequiredNoticeId> always_show_after) {
  CHECK(notice_id);
  CHECK(!ready_to_start_callback.is_null());
  const auto result = data_.emplace(
      notice_id, RequiredNoticeData(std::move(ready_to_start_callback),
                                    std::move(always_show_after)));
  CHECK(result.second) << "Duplicate required notice ID: " << notice_id;
  MaybeShowNextRequiredNotice();
}

void ProductMessagingController::UnqueueRequiredNotice(
    RequiredNoticeId notice_id) {
  data_.erase(notice_id);
}

void ProductMessagingController::ReleaseHandle(RequiredNoticeId notice_id) {
  CHECK_EQ(current_notice_, notice_id);
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

void ProductMessagingController::MaybeShowNextRequiredNoticeImpl() {
  if (!ready_to_show()) {
    return;
  }

  // Find a notice that is not waiting for any other notices to show.
  RequiredNoticeId to_show;
  for (const auto& [id, data] : data_) {
    bool excluded = false;
    bool show_after_all = false;
    for (auto after : data.show_after) {
      if (after == internal::kShowAfterAllNotices) {
        show_after_all = true;
      } else if (base::Contains(data_, after)) {
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
  RequiredNoticeShowCallback cb = std::move(data_[to_show].callback);
  data_.erase(to_show);
  current_notice_ = to_show;
  std::move(cb).Run(
      RequiredNoticePriorityHandle(to_show, weak_ptr_factory_.GetWeakPtr()));
}

std::string ProductMessagingController::DumpData() const {
  std::ostringstream oss;
  for (const auto& [id, data] : data_) {
    oss << "\n{ id: " << id << " show_after: { ";
    for (const auto& after : data.show_after) {
      oss << after << ", ";
    }
    oss << "} }";
  }
  return oss.str();
}

}  // namespace user_education
