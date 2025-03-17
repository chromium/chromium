// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/messaging_coordinator.h"

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "components/user_education/common/product_messaging_controller.h"

namespace user_education::internal {

DEFINE_CLASS_REQUIRED_NOTICE_IDENTIFIER(MessagingCoordinator,
                                        kLowPriorityNoticeId);
DEFINE_CLASS_REQUIRED_NOTICE_IDENTIFIER(MessagingCoordinator,
                                        kHighPriorityNoticeId);

MessagingCoordinator::MessagingCoordinator(
    ProductMessagingController& controller)
    : controller_(controller) {
  message_shown_subscription_ = controller_->AddRequiredNoticeShownCallback(
      base::BindRepeating(&MessagingCoordinator::OnMessageShown,
                          weak_ptr_factory_.GetWeakPtr()));
}

MessagingCoordinator::~MessagingCoordinator() = default;

bool MessagingCoordinator::CanShowPromo(bool high_priority) const {
  // Must always be holding the handle.
  if (!handle_) {
    return false;
  }

  // Only a high priority promo can take advantage of a high-priority-pending.
  if (!high_priority && (promo_state_ == PromoState::kHighPriorityPending ||
                         promo_state_ == PromoState::kHighPriorityShowing)) {
    return false;
  }

  // Otherwise holding the handle is sufficient.
  return true;
}

bool MessagingCoordinator::IsBlockedByExternalPromo() const {
  return !handle_ && controller_->has_current_notice();
}

void MessagingCoordinator::TransitionToState(PromoState promo_state) {
  switch (promo_state) {
    case PromoState::kNone:
      ReleaseAll();
      break;
    case PromoState::kLowPriorityShowing:
      CHECK(CanShowPromo(false));
      // Priority is not held when a low priority promo is showing.
      handle_.Release();
      break;
    case PromoState::kHighPriorityShowing:
      CHECK(CanShowPromo(true));
      break;
    case PromoState::kLowPriorityPending:
      MaybeRequestPriority(/*high_priority=*/false);
      break;
    case PromoState::kHighPriorityPending:
      MaybeRequestPriority(/*high_priority=*/true);
      break;
  }

  promo_state_ = promo_state;
}

base::CallbackListSubscription MessagingCoordinator::AddPromoPreemptedCallback(
    base::RepeatingClosure callback) {
  return promo_preempted_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription MessagingCoordinator::AddPromoReadyCallback(
    base::RepeatingClosure callback) {
  return promo_ready_callbacks_.Add(std::move(callback));
}

void MessagingCoordinator::MaybeRequestPriority(bool high_priority) {
  // Should not re-request priority if we already have it. If the promo can be
  // shown it should either be shown, or the state should return to kNone.
  CHECK(!CanShowPromo(high_priority));

  // If the handle is held but it's being held for the wrong reason, release it.
  if (handle_) {
    handle_.Release();
  }

  auto cb = base::BindOnce(&MessagingCoordinator::OnPriorityReceived,
                           weak_ptr_factory_.GetWeakPtr());
  if (high_priority) {
    // High priority notices take the same precedence as
    if (!controller_->IsNoticeQueued(kHighPriorityNoticeId)) {
      controller_->UnqueueRequiredNotice(kLowPriorityNoticeId);
      controller_->QueueRequiredNotice(kHighPriorityNoticeId, std::move(cb));
    }
  } else if (!controller_->IsNoticeQueued(kLowPriorityNoticeId)) {
    // Low-priority show after all other messaging.
    controller_->UnqueueRequiredNotice(kHighPriorityNoticeId);
    controller_->QueueRequiredNotice(kLowPriorityNoticeId, std::move(cb),
                                     {kShowAfterAllNotices});
  }
}

void MessagingCoordinator::ReleaseAll() {
  handle_.Release();
  controller_->UnqueueRequiredNotice(kLowPriorityNoticeId);
  controller_->UnqueueRequiredNotice(kHighPriorityNoticeId);
}

void MessagingCoordinator::OnPriorityReceived(
    RequiredNoticePriorityHandle handle) {
  handle_ = std::move(handle);
  promo_ready_callbacks_.Notify();
}

void MessagingCoordinator::OnMessageShown(RequiredNoticeId message_id) {
  if (message_id == kLowPriorityNoticeId ||
      message_id == kHighPriorityNoticeId) {
    return;
  }
  if (promo_state_ == PromoState::kLowPriorityShowing) {
    promo_preempted_callbacks_.Notify();
  }
}

}  // namespace user_education::internal
