// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager_impl.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"

namespace autofill {

namespace {
// The timeout after which the bubble can be replaced if now shown.
constexpr base::TimeDelta kPendingRequestTimeout = base::Seconds(3600);

// Helper to get the priority for a given bubble `type`. Higher number
// indicates higher priority.
int GetPriorityForBubbleType(BubbleType type) {
  switch (type) {
    case BubbleType::kFilledCardInformation:
      return 10;
    case BubbleType::kPassword:
      return 9;
    case BubbleType::kSaveUpdateAutofillAi:
      return 8;
    case BubbleType::kSaveUpdateCard:
      return 7;
    case BubbleType::kVirtualCardEnrollConfirmation:
      return 6;
    case BubbleType::kSaveIban:
      return 5;
    case BubbleType::kMandatoryReauth:
      return 4;
    case BubbleType::kSaveUpdateAddress:
      return 3;
    case BubbleType::kOfferNotification:
      return 2;
  }
  NOTREACHED();
}

// Returns true if a new bubble of this type should always replace an
// existing pending bubble of the same type in the queue.
bool ShouldAlwaysPreemptSameType(BubbleType bubble_type) {
  switch (bubble_type) {
    case BubbleType::kFilledCardInformation:
    case BubbleType::kPassword:
      return true;
    case BubbleType::kSaveUpdateAutofillAi:
    case BubbleType::kSaveUpdateCard:
    case BubbleType::kVirtualCardEnrollConfirmation:
    case BubbleType::kSaveIban:
    case BubbleType::kMandatoryReauth:
    case BubbleType::kSaveUpdateAddress:
    case BubbleType::kOfferNotification:
      return false;
  }
  NOTREACHED();
}

}  // namespace

BubbleManagerImpl::PendingRequest::PendingRequest(
    base::WeakPtr<BubbleControllerBase> controller,
    base::TimeTicks time_added,
    int priority)
    : controller(std::move(controller)),
      time_added(time_added),
      priority(priority) {}
BubbleManagerImpl::PendingRequest::~PendingRequest() = default;
BubbleManagerImpl::PendingRequest::PendingRequest(const PendingRequest& other) =
    default;
BubbleManagerImpl::PendingRequest& BubbleManagerImpl::PendingRequest::operator=(
    const PendingRequest& other) = default;

bool BubbleManagerImpl::PendingRequest::operator<(
    const BubbleManagerImpl::PendingRequest& other) const {
  if (priority != other.priority) {
    return priority > other.priority;
  }

  // If priorities are equal, the one that arrived first should be shown first.
  return time_added < other.time_added;
}

BubbleManagerImpl::BubbleManagerImpl() = default;

BubbleManagerImpl::~BubbleManagerImpl() = default;

void BubbleManagerImpl::RequestShowController(
    BubbleControllerBase& controller_to_show,
    bool force_show) {
  base::WeakPtr<BubbleControllerBase> controller_weak_ptr =
      controller_to_show.GetBubbleControllerBaseWeakPtr();

  base::AutoReset<bool> show_request_guard(&handling_show_request_, true);

  if (!active_bubble_controller_) {
    // No active bubble, so this one can be shown immediately.
    ShowAndSetCurrentActive(controller_weak_ptr);
    return;
  }

  if (force_show ||
      ShouldReplaceExistingBubble(controller_weak_ptr->GetBubbleType())) {
    HideActiveBubbleForPreemption();

    // Immediately show the new, preempting bubble.
    ShowAndSetCurrentActive(controller_weak_ptr);
  } else {
    // New bubble has lower or equal priority, or the active bubble is hovered;
    // queue it.
    AddToPendingQueue(controller_weak_ptr);
  }
}

void BubbleManagerImpl::ShowAndSetCurrentActive(
    base::WeakPtr<BubbleControllerBase> controller_to_show) {
  CHECK(controller_to_show);
  active_bubble_controller_ = controller_to_show;

  for (auto it = pending_bubbles_queue_.begin();
       it != pending_bubbles_queue_.end(); ++it) {
    if (it->controller.get() == controller_to_show.get()) {
      // Remove from the pending queue if it was there.
      pending_bubbles_queue_.erase(it);
      break;
    }
  }

  active_bubble_controller_->ShowBubble();
}

void BubbleManagerImpl::HideActiveBubbleForPreemption() {
  CHECK(active_bubble_controller_ &&
        active_bubble_controller_->IsShowingBubble());
  CHECK(handling_show_request_);

  // Queue the old bubble. It will be hidden, and its OnBubbleHiddenByController
  // call will be a no-op for starting the next bubble because we are inside a
  // show request (`handling_show_request_` is true).
  AddToPendingQueue(active_bubble_controller_);
  active_bubble_controller_->HideBubble();
}

void BubbleManagerImpl::AddToPendingQueue(
    base::WeakPtr<BubbleControllerBase> controller) {
  CHECK(controller);
  const BubbleType new_bubble_type = controller->GetBubbleType();
  const base::TimeTicks now = base::TimeTicks::Now();
  int priority = GetPriorityForBubbleType(new_bubble_type);

  auto it = std::ranges::find_if(
      pending_bubbles_queue_,
      [&new_bubble_type](const PendingRequest& request) {
        return request.controller &&
               request.controller->GetBubbleType() == new_bubble_type;
      });

  if (it != pending_bubbles_queue_.end()) {
    // If a bubble of the same type exists, erase it before inserting the new
    // one if the controller says so or if it has timed out.
    if (ShouldAlwaysPreemptSameType(new_bubble_type) ||
        (now - it->time_added) > kPendingRequestTimeout) {
      pending_bubbles_queue_.erase(it);
      pending_bubbles_queue_.insert(PendingRequest(controller, now, priority));
    }
  } else {
    // No bubble of this type exists, just insert it.
    pending_bubbles_queue_.insert(PendingRequest(controller, now, priority));
  }
}

void BubbleManagerImpl::ProcessPendingBubbles() {
  if (handling_show_request_ ||
      (active_bubble_controller_ &&
       active_bubble_controller_->IsShowingBubble())) {
    // The bubble is hidden due to preemption and added to the queue. Therefore,
    // do not show any new bubbles.
    return;
  }

  // Clean up any stale pointers and timed out bubbles.
  const base::TimeTicks now = base::TimeTicks::Now();
  std::erase_if(pending_bubbles_queue_, [&now](const auto& request) {
    return !request.controller ||
           (now - request.time_added) > kPendingRequestTimeout;
  });

  if (pending_bubbles_queue_.empty()) {
    active_bubble_controller_ = nullptr;
    return;
  }

  auto next_controller_to_show = pending_bubbles_queue_.begin()->controller;
  pending_bubbles_queue_.erase(pending_bubbles_queue_.begin());

  // Show the next bubble from the queue.
  ShowAndSetCurrentActive(next_controller_to_show);
}

void BubbleManagerImpl::OnBubbleHiddenByController(
    BubbleControllerBase& controller_to_hide) {
  base::WeakPtr<BubbleControllerBase> controller_weak_ptr =
      controller_to_hide.GetBubbleControllerBaseWeakPtr();
  if (active_bubble_controller_.get() == controller_weak_ptr.get()) {
    active_bubble_controller_ = nullptr;
    ProcessPendingBubbles();
  } else {
    // The hidden bubble was not the active one, so remove it from the queue.
    for (auto it = pending_bubbles_queue_.begin();
         it != pending_bubbles_queue_.end(); ++it) {
      if (it->controller.get() == controller_weak_ptr.get()) {
        pending_bubbles_queue_.erase(it);
        break;
      }
    }
  }
}

bool BubbleManagerImpl::HasPendingBubbleOfSameType(
    const BubbleType bubble_type) const {
  const base::TimeTicks now = base::TimeTicks::Now();

  auto it = std::ranges::find_if(
      pending_bubbles_queue_, [bubble_type](const PendingRequest& request) {
        // Check if the controller is still valid and if its type
        // matches.
        return request.controller &&
               request.controller->GetBubbleType() == bubble_type;
      });

  // If no bubble of the specified type is found in the queue.
  if (it == pending_bubbles_queue_.end()) {
    return false;
  }

  return !ShouldAlwaysPreemptSameType(bubble_type) &&
         (now - it->time_added) < kPendingRequestTimeout;
}

bool BubbleManagerImpl::ShouldReplaceExistingBubble(
    const BubbleType new_bubble_type) const {
  if (active_bubble_controller_->IsMouseHovered()) {
    return false;
  }

  const BubbleType active_bubble_type =
      active_bubble_controller_->GetBubbleType();

  // If the bubbles have same type, preempt based on the controller type.
  if (new_bubble_type == active_bubble_type) {
    return ShouldAlwaysPreemptSameType(new_bubble_type);
  }

  // Otherwise, preempt based on priority.
  return GetPriorityForBubbleType(new_bubble_type) >
         GetPriorityForBubbleType(active_bubble_type);
}

}  // namespace autofill
