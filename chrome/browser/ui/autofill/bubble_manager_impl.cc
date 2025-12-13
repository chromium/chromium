// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager_impl.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "components/tabs/public/tab_interface.h"

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
    case BubbleType::kWalletablePassConsent:
      return 1;
    case BubbleType::kWalletablePassSave:
      return 0;
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
    case BubbleType::kWalletablePassConsent:
    case BubbleType::kWalletablePassSave:
      return false;
  }
  NOTREACHED();
}

// LINT.IfChange(BubbleTypeToMetricSuffix)
std::string_view BubbleTypeToMetricSuffix(BubbleType bubble_type) {
  switch (bubble_type) {
    case BubbleType::kSaveUpdateAddress:
      return "SaveUpdateAddress";
    case BubbleType::kSaveIban:
      return "SaveIban";
    case BubbleType::kSaveUpdateCard:
      return "SaveUpdateCard";
    case BubbleType::kSaveUpdateAutofillAi:
      return "SaveUpdateAutofillAi";
    case BubbleType::kVirtualCardEnrollConfirmation:
      return "VirtualCardEnrollConfirmation";
    case BubbleType::kMandatoryReauth:
      return "MandatoryReauth";
    case BubbleType::kOfferNotification:
      return "OfferNotification";
    case BubbleType::kFilledCardInformation:
      return "FilledCardInformation";
    case BubbleType::kPassword:
      return "Password";
    case BubbleType::kWalletablePassConsent:
      return "WalletablePassConsent";
    case BubbleType::kWalletablePassSave:
      return "WalletablePassSave";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.Bubble.Queue.TimeInQueue.BubbleType)

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

BubbleManagerImpl::BubbleManagerImpl(tabs::TabInterface* tab)
    : tab_interface_(tab) {
  tab_subscriptions_.push_back(tab->RegisterWillDeactivate(base::BindRepeating(
      &BubbleManagerImpl::TabWillEnterBackground, base::Unretained(this))));
  tab_subscriptions_.push_back(tab->RegisterDidActivate(base::BindRepeating(
      &BubbleManagerImpl::TabDidEnterForeground, base::Unretained(this))));
}

BubbleManagerImpl::~BubbleManagerImpl() {
  std::ranges::for_each(pending_bubbles_queue_, [](const auto& request) {
    if (request.controller) {
      request.controller->OnBubbleDiscarded();
    }
  });
}

void BubbleManagerImpl::RequestShowController(
    BubbleControllerBase& controller_to_show,
    bool force_show) {
  base::WeakPtr<BubbleControllerBase> controller_weak_ptr =
      controller_to_show.GetBubbleControllerBaseWeakPtr();

  if (!tab_interface_->IsActivated()) {
    AddToPendingQueue(controller_weak_ptr);
    return;
  }

  if (force_show) {
    base::UmaHistogramEnumeration("Autofill.Bubble.RequestShow.ForceShow",
                                  controller_to_show.GetBubbleType());
  }

  base::UmaHistogramEnumeration("Autofill.Bubble.RequestShow",
                                controller_to_show.GetBubbleType());

  base::AutoReset<bool> show_request_guard(&handling_show_request_, true);

  if (!active_bubble_controller_ ||
      !active_bubble_controller_->IsShowingBubble()) {
    // No active bubble, so this one can be shown immediately.
    base::UmaHistogramEnumeration("Autofill.Bubble.Show.NoActiveBubble",
                                  controller_to_show.GetBubbleType());
    ShowAndSetCurrentActive(controller_weak_ptr);
    return;
  }

  if (force_show ||
      ShouldReplaceExistingBubble(controller_to_show.GetBubbleType())) {
    base::UmaHistogramEnumeration("Autofill.Bubble.Show.Preemption",
                                  controller_to_show.GetBubbleType());
    HideActiveBubbleForPreemption();
    ShowAndSetCurrentActive(controller_weak_ptr);
    return;
  }

  // Queue the bubble. Log the reason for queuing.
  if (active_bubble_controller_->IsMouseHovered()) {
    base::UmaHistogramEnumeration("Autofill.Bubble.Queue.AddedDueToHover",
                                  controller_to_show.GetBubbleType());
  } else {
    base::UmaHistogramEnumeration(
        "Autofill.Bubble.Queue.AddedDueToActiveBubble",
        controller_to_show.GetBubbleType());
  }
  AddToPendingQueue(controller_weak_ptr);
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

  base::UmaHistogramEnumeration("Autofill.Bubble.WasPreempted",
                                active_bubble_controller_->GetBubbleType());

  // Queue the old bubble. It will be hidden, and its OnBubbleHiddenByController
  // call will be a no-op for starting the next bubble because we are inside a
  // show request (`handling_show_request_` is true).
  AddToPendingQueue(active_bubble_controller_);
  active_bubble_controller_->HideBubble(/*initiated_by_bubble_manager=*/true);
}

void BubbleManagerImpl::AddToPendingQueue(
    base::WeakPtr<BubbleControllerBase> controller) {
  CHECK(controller);
  if (!controller->CanBeReshown()) {
    // Bubbles that cannot be reshown should not be added to the queue. This is
    // the case for bubbles that are time-bound or clear their state when
    // closed, mostly the confirmation bubbles.
    return;
  }

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
    const bool bubble_has_timed_out =
        (now - it->time_added) > kPendingRequestTimeout;
    if (ShouldAlwaysPreemptSameType(new_bubble_type) || bubble_has_timed_out) {
      if (bubble_has_timed_out) {
        if (it->controller) {
          it->controller->OnBubbleDiscarded();
          base::UmaHistogramEnumeration("Autofill.Bubble.Queue.TimedOut",
                                        it->controller->GetBubbleType());
        }
      } else {
        base::UmaHistogramEnumeration("Autofill.Bubble.Queue.Replaced",
                                      new_bubble_type);
      }
      pending_bubbles_queue_.erase(it);
      pending_bubbles_queue_.insert(PendingRequest(controller, now, priority));
    } else {
      base::UmaHistogramEnumeration("Autofill.Bubble.Queue.Discarded",
                                    new_bubble_type);
    }
  } else {
    // No bubble of this type exists, just insert it.
    pending_bubbles_queue_.insert(PendingRequest(controller, now, priority));
  }
}

void BubbleManagerImpl::ProcessPendingBubbles(bool tab_entered_foreground) {
  if (handling_show_request_ || handling_tab_will_enter_background_request_ ||
      (active_bubble_controller_ &&
       active_bubble_controller_->IsShowingBubble())) {
    // The bubble is hidden due to preemption and added to the queue. Or the tab
    // is about to hide. Therefore, do not show any new bubbles.
    return;
  }

  // Clean up any stale pointers and timed out bubbles.
  const base::TimeTicks now = base::TimeTicks::Now();
  std::ranges::for_each(pending_bubbles_queue_, [&now](const auto& request) {
    if (request.controller &&
        (now - request.time_added) > kPendingRequestTimeout) {
      request.controller->OnBubbleDiscarded();
      // Log timed-out bubbles.
      base::UmaHistogramEnumeration("Autofill.Bubble.Queue.TimedOut",
                                    request.controller->GetBubbleType());
    }
  });
  std::erase_if(pending_bubbles_queue_, [&now](const auto& request) {
    return !request.controller ||
           (now - request.time_added) > kPendingRequestTimeout;
  });

  if (pending_bubbles_queue_.empty()) {
    active_bubble_controller_ = nullptr;
    return;
  }

  auto it = pending_bubbles_queue_.begin();
  base::WeakPtr<BubbleControllerBase> next_controller_to_show = it->controller;
  base::TimeDelta time_in_queue = now - it->time_added;
  pending_bubbles_queue_.erase(it);

  std::string bubble_shown_metric = "Autofill.Bubble.Queue.ShownFromQueue";
  if (tab_entered_foreground) {
    bubble_shown_metric += "OnTabVisible";
  }
  base::UmaHistogramEnumeration(bubble_shown_metric,
                                next_controller_to_show->GetBubbleType());
  base::UmaHistogramTimes(
      base::StrCat(
          {"Autofill.Bubble.Queue.TimeInQueue.",
           BubbleTypeToMetricSuffix(next_controller_to_show->GetBubbleType())}),
      time_in_queue);
  // Show the next bubble from the queue.
  ShowAndSetCurrentActive(next_controller_to_show);
}

void BubbleManagerImpl::OnBubbleHiddenByController(
    BubbleControllerBase& controller_to_hide,
    bool show_next_bubble) {
  base::WeakPtr<BubbleControllerBase> controller_weak_ptr =
      controller_to_hide.GetBubbleControllerBaseWeakPtr();

  if (active_bubble_controller_.get() == controller_weak_ptr.get()) {
    active_bubble_controller_ = nullptr;
    if (show_next_bubble) {
      ProcessPendingBubbles(/*tab_entered_foreground=*/false);
    }
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
        return request.controller &&
               request.controller->GetBubbleType() == bubble_type;
      });

  return it != pending_bubbles_queue_.end() &&
         (now - it->time_added) < kPendingRequestTimeout;
}

bool BubbleManagerImpl::HasConflictingPendingBubble(
    const BubbleType bubble_type) const {
  // If this type always preempts itself (like Passwords), it never blocks
  // a new request of the same type.
  if (ShouldAlwaysPreemptSameType(bubble_type)) {
    return false;
  }

  return HasPendingBubbleOfSameType(bubble_type);
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

void BubbleManagerImpl::TabWillEnterBackground(
    tabs::TabInterface* tab_interface) {
  base::AutoReset<bool> hide_request_guard(
      &handling_tab_will_enter_background_request_, true);
  if (active_bubble_controller_) {
    base::UmaHistogramEnumeration("Autofill.Bubble.HideDueToTabHide",
                                  active_bubble_controller_->GetBubbleType());
    AddToPendingQueue(active_bubble_controller_);
    active_bubble_controller_->HideBubble(/*initiated_by_bubble_manager=*/true);
    active_bubble_controller_ = nullptr;
  }
}

void BubbleManagerImpl::TabDidEnterForeground(
    tabs::TabInterface* tab_interface) {
  if (!active_bubble_controller_) {
    ProcessPendingBubbles(/*tab_entered_foreground=*/true);
  } else if (!active_bubble_controller_->IsShowingBubble()) {
    // This can happen if a tab created in background becomes visible.
    active_bubble_controller_->ShowBubble();
  }
}

}  // namespace autofill
