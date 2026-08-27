// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/chip_selector.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"
#include "components/user_education/product_messaging/product_messaging_types.h"
#include "ui/actions/action_id.h"

namespace page_actions {

namespace internal {

DEFINE_CLASS_PRODUCT_MESSAGE_KEY(PriorityChipSelector, kAnchoredMessageId);
DefaultChipSelector::DefaultChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId,
                                 const AnchoredMessageConfig&)>
        show_anchored_message_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        hide_anchored_message_callback)
    : show_chip_callback_(show_chip_callback),
      hide_chip_callback_(hide_chip_callback),
      show_anchored_message_callback_(show_anchored_message_callback),
      hide_anchored_message_callback_(hide_anchored_message_callback) {}

DefaultChipSelector::~DefaultChipSelector() = default;

void DefaultChipSelector::RequestChipShow(actions::ActionId page_action_id,
                                          const SuggestionChipConfig& config) {
  // Manual User Action is only supported for anchored messages
  CHECK(config.priority != PageActionPriorityCategory::kUserInteraction);
  if (!active_chips_.contains(page_action_id)) {
    active_chips_.insert(page_action_id);
    base::UmaHistogramExactLinear("PageActionController.ActiveSuggestionChips",
                                  active_chips_.size(), 25);
  }
  show_chip_callback_.Run(page_action_id, config);
  RequestAnchoredMessageHide(page_action_id);
}
void DefaultChipSelector::RequestChipHide(actions::ActionId page_action_id) {
  active_chips_.erase(page_action_id);
  hide_chip_callback_.Run(page_action_id);
}

void DefaultChipSelector::RequestAnchoredMessageShow(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
  if (config.priority == PageActionPriorityCategory::kUserInteraction) {
    // If this request comes from an explicit user action, if there is no
    // anchored message showing, we just show it. If there is another anchored
    // message showing, we downgrade it to a chip and then show the newly
    // requested message with no other changes to the queue.
    if (std::ranges::contains(active_chips_, page_action_id)) {
      active_chips_.erase(page_action_id);
      hide_chip_callback_.Run(page_action_id);
    }
    if (anchored_message_queue_.empty()) {
      // If no anchored messages are showing, put this one in the queue
      anchored_message_queue_.push_back(page_action_id);
    } else if (anchored_message_queue_[0] == page_action_id) {
      // This anchored message is already showing. We trigger the callback so
      // that the message doesn't time out.
      show_anchored_message_callback_.Run(page_action_id, config);
      return;
    } else {
      // Downgrade anchored message
      hide_anchored_message_callback_.Run(anchored_message_queue_[0]);
      show_chip_callback_.Run(anchored_message_queue_[0], {});
      // Remove this page action from the queue (if it's there).
      std::erase(anchored_message_queue_, page_action_id);
      anchored_message_queue_[0] = page_action_id;
    }
    show_anchored_message_callback_.Run(page_action_id, config);
    return;
  }
  if (std::ranges::contains(anchored_message_queue_, page_action_id)) {
    // This page action's anchored message is already queued. Nothing to do.
    return;
  }
  // Enqueue the page action's anchored message.
  anchored_message_queue_.push_back(page_action_id);
  if (anchored_message_queue_.size() > 1) {
    // Other messages ahead of it in the queue. Do not show the new one.
    return;
  }
  show_anchored_message_callback_.Run(page_action_id, config);
  if (active_chips_.contains(page_action_id)) {
    RequestChipHide(page_action_id);
  }
}

void DefaultChipSelector::RequestAnchoredMessageHide(
    actions::ActionId page_action_id) {
  auto it = std::find(anchored_message_queue_.begin(),
                      anchored_message_queue_.end(), page_action_id);

  if (it == anchored_message_queue_.end()) {
    // Anchored message not queued.
    return;
  }
  bool is_active = (it == anchored_message_queue_.begin());
  anchored_message_queue_.erase(it);
  if (!is_active) {
    // Anchored message queued, but not shown.
    return;
  }
  // Hide anchored message.
  hide_anchored_message_callback_.Run(page_action_id);
  if (anchored_message_queue_.size() > 0) {
    // Show the next anchored message in queue.
    show_anchored_message_callback_.Run(anchored_message_queue_[0], {});
  }
}

void DefaultChipSelector::OnTabActiveChanged(bool is_tab_active) {}

struct PriorityChipSelector::PendingAnchoredMessage {
  actions::ActionId page_action_id;
  AnchoredMessageConfig config;
};

PriorityChipSelector::PriorityChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId,
                                 const AnchoredMessageConfig&)>
        show_anchored_message_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        hide_anchored_message_callback,
    user_education::ProductMessagingController* product_messaging_controller)
    : show_chip_callback_(show_chip_callback),
      hide_chip_callback_(hide_chip_callback),
      show_anchored_message_callback_(show_anchored_message_callback),
      hide_anchored_message_callback_(hide_anchored_message_callback),
      product_messaging_controller_(product_messaging_controller) {}

PriorityChipSelector::~PriorityChipSelector() {
  CancelPendingAnchoredMessage();
}

void PriorityChipSelector::RequestChipShow(actions::ActionId page_action_id,
                                           const SuggestionChipConfig& config) {
  // Manual User Action is only supported for anchored messages
  CHECK(config.priority != PageActionPriorityCategory::kUserInteraction);
  if (active_chips_.contains(page_action_id)) {
    // This chip is already showing, but may have been triggered at a different
    // priority.
    if (active_priority_ == config.priority) {
      // Same priority - nothing to do.
      return;
    }
    // Different priority, hide, then reshow
    RequestChipHide(page_action_id);
  }
  if (pending_anchored_message_ &&
      pending_anchored_message_->page_action_id == page_action_id) {
    CancelPendingAnchoredMessage();
  }
  if (active_anchored_message_ == page_action_id) {
    // This action is currently showing an anchored message. Hide that, then
    // show chip.
    RequestAnchoredMessageHide(page_action_id);
  }

  if (!active_priority_) {
    // No active suggestion chip or anchored message, so we show the request.
    ShowChip(page_action_id, config);
    return;
  }

  if (config.priority <= active_priority_ &&
      config.priority < PageActionPriorityCategory::kPrivacySecurity) {
    // Active suggestion chip or anchored message is either of higher priority,
    // or of the same priority, which is not Privacy/Security, so we don't show
    // the new one.
    return;
  }

  if (active_priority_ < config.priority) {
    // Active suggestion chip or anchored message is of lower priority. Hide it
    // and show the new request.
    HideAllActive();
    ShowChip(page_action_id, config);
    return;
  }

  // Final case: active suggestion chip or anchored message, and requested one
  // are both Privacy/Security, so we show the newly requested chip alongside
  // the existing one(s).
  ShowChip(page_action_id, config);
}

void PriorityChipSelector::RequestChipHide(actions::ActionId page_action_id) {
  if (!active_chips_.contains(page_action_id)) {
    return;
  }
  active_chips_.erase(page_action_id);
  hide_chip_callback_.Run(page_action_id);
  if (active_chips_.empty() && !active_anchored_message_ &&
      !pending_anchored_message_) {
    active_priority_.reset();
  }
}

void PriorityChipSelector::RequestAnchoredMessageShow(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
  if (base::FeatureList::IsEnabled(
          features::kPageActionAnchoredMessageActiveTabOnly) &&
      !is_tab_active_ &&
      config.priority != PageActionPriorityCategory::kUserInteraction) {
    RequestChipShow(page_action_id, {.priority = config.priority});
    return;
  }
  if (active_anchored_message_ == page_action_id) {
    // This anchored message is already showing, but possibly at a different
    // priority.
    if (active_priority_ == config.priority) {
      // Same priority - nothing to do.
      return;
    }
    // Different priority - hide, then reshow
    RequestAnchoredMessageHide(page_action_id);
  }
  if (pending_anchored_message_ &&
      pending_anchored_message_->page_action_id == page_action_id) {
    if (active_priority_ == config.priority) {
      return;
    }
    CancelPendingAnchoredMessage();
  }
  if (active_chips_.contains(page_action_id)) {
    // This page action is currently showing a suggestion chip. Hide it and then
    // attempt to show the anchored message.
    RequestChipHide(page_action_id);
  }

  if (!active_priority_) {
    // No active suggestion chip or anchored message, so we show or queue the
    // request.
    MaybeShowOrQueueAnchoredMessage(page_action_id, config);
    return;
  }

  if (config.priority <= active_priority_ &&
      config.priority < PageActionPriorityCategory::kPrivacySecurity) {
    // We always show privacy/security or user interaction requests in a
    // possibly downgraded state. Otherwise, we only show higher priority ones.
    return;
  }

  if (active_priority_ < config.priority &&
      active_priority_ < PageActionPriorityCategory::kPrivacySecurity) {
    // Active suggestion chip or anchored message is of lower priority. Hide it
    // unless it is a privacy/security one.
    HideAllActive();
    MaybeShowOrQueueAnchoredMessage(page_action_id, config);
    return;
  } else if (config.priority == PageActionPriorityCategory::kUserInteraction) {
    // User interaction -> downgrade visible anchored message (if any) to a
    // suggestion chip, and show the requested one.
    CancelPendingAnchoredMessage();
    if (active_anchored_message_) {
      pmc_handle_.reset();
      hide_anchored_message_callback_.Run(active_anchored_message_.value());
      show_chip_callback_.Run(active_anchored_message_.value(),
                              {.priority = active_priority_.value()});
      active_anchored_message_.reset();
    }
    active_priority_ = config.priority;
    ShowAnchoredMessage(page_action_id, config);
    return;
  }

  // Final case: active suggestion chip or anchored message is either
  // Privacy/Security or User Interaction, and requested one is
  // Privacy/Security. If we are already showing or pending an anchored message,
  // the new request is downgraded to a suggestion chip, otherwise, we show or
  // queue it.
  if (active_anchored_message_ || pending_anchored_message_) {
    ShowChip(page_action_id,
             SuggestionChipConfig{
                 .priority = PageActionPriorityCategory::kPrivacySecurity});
  } else {
    MaybeShowOrQueueAnchoredMessage(page_action_id, config);
  }
}

void PriorityChipSelector::RequestAnchoredMessageHide(
    actions::ActionId page_action_id) {
  if (pending_anchored_message_ &&
      pending_anchored_message_->page_action_id == page_action_id) {
    CancelPendingAnchoredMessage();
    return;
  }
  if (active_anchored_message_ != page_action_id) {
    return;
  }
  pmc_handle_.reset();
  active_anchored_message_.reset();
  hide_anchored_message_callback_.Run(page_action_id);

  if (active_chips_.empty() && !pending_anchored_message_) {
    active_priority_.reset();
  }
}

void PriorityChipSelector::OnTabActiveChanged(bool is_tab_active) {
  is_tab_active_ = is_tab_active;
  if (is_tab_active_ ||
      !base::FeatureList::IsEnabled(
          features::kPageActionAnchoredMessageActiveTabOnly)) {
    return;
  }

  if (pending_anchored_message_) {
    actions::ActionId pending_id = pending_anchored_message_->page_action_id;
    PageActionPriorityCategory priority =
        pending_anchored_message_->config.priority;
    CancelPendingAnchoredMessage();
    ShowChip(pending_id, {.priority = priority});
    return;
  }

  if (!active_anchored_message_ ||
      active_priority_ == PageActionPriorityCategory::kUserInteraction) {
    return;
  }

  pmc_handle_.reset();
  actions::ActionId page_action_id = active_anchored_message_.value();
  PageActionPriorityCategory priority = active_priority_.value();
  hide_anchored_message_callback_.Run(page_action_id);
  active_anchored_message_.reset();
  ShowChip(page_action_id, {.priority = priority});
}

void PriorityChipSelector::HideAllActive() {
  CancelPendingAnchoredMessage();
  for (const auto chip_id : active_chips_) {
    hide_chip_callback_.Run(chip_id);
  }
  active_chips_.clear();
  if (active_anchored_message_) {
    pmc_handle_.reset();
    hide_anchored_message_callback_.Run(active_anchored_message_.value());
  }
  active_anchored_message_.reset();
  active_priority_.reset();
}

void PriorityChipSelector::ShowChip(actions::ActionId page_action_id,
                                    const SuggestionChipConfig& config) {
  // We verify that either there is no active priority, it has already been set
  // to the same level as requested, or the request is Privacy/Security or
  // higher, which allows multiple items to show at once.
  CHECK(!active_priority_ || active_priority_ == config.priority ||
        config.priority >= PageActionPriorityCategory::kPrivacySecurity);
  active_chips_.insert(page_action_id);
  show_chip_callback_.Run(page_action_id, config);
  if (!active_priority_ || active_priority_ == config.priority) {
    active_priority_ = config.priority;
  }
}

void PriorityChipSelector::ShowAnchoredMessage(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
  CHECK(!active_anchored_message_);
  // We verify that either there is no active priority, it has already been set
  // to the same level as requested, or the request is Privacy/Security or
  // higher, which allows multiple items to show at once.
  CHECK(!active_priority_ || active_priority_ == config.priority ||
        config.priority >= PageActionPriorityCategory::kPrivacySecurity);
  active_anchored_message_ = page_action_id;
  show_anchored_message_callback_.Run(page_action_id, config);
  active_priority_ = config.priority;
}

bool PriorityChipSelector::ShouldUsePmc(
    const AnchoredMessageConfig& config) const {
  // The anchored message active tab flag check is needed to avoid 2 different
  // tabs trying to schedule anchored messages at the same time.
  return base::FeatureList::IsEnabled(
             features::
                 kPageActionsPrioritySelectorProductMessagingController) &&
         base::FeatureList::IsEnabled(
             features::kPageActionAnchoredMessageActiveTabOnly) &&
         product_messaging_controller_ != nullptr &&
         config.priority != PageActionPriorityCategory::kUserInteraction;
}

void PriorityChipSelector::MaybeShowOrQueueAnchoredMessage(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
  if (ShouldUsePmc(config)) {
    QueueAnchoredMessage(page_action_id, config);
  } else {
    ShowAnchoredMessage(page_action_id, config);
  }
}

void PriorityChipSelector::QueueAnchoredMessage(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
  CHECK(!pending_anchored_message_);
  CHECK(!active_anchored_message_);
  CHECK(product_messaging_controller_);
  pending_anchored_message_ =
      std::make_unique<PendingAnchoredMessage>(page_action_id, config);
  active_priority_ = config.priority;

  pmc_timeout_timer_.Start(
      FROM_HERE, kPmcTimeout,
      base::BindOnce(&PriorityChipSelector::OnPmcTimeout,
                     weak_ptr_factory_.GetWeakPtr(), page_action_id));

  product_messaging_controller_->QueueMessage(
      kAnchoredMessageId,
      base::BindOnce(&PriorityChipSelector::OnPmcPermissionGranted,
                     weak_ptr_factory_.GetWeakPtr(), page_action_id),
      kPmcTimeout);
}

void PriorityChipSelector::CancelPendingAnchoredMessage() {
  if (!pending_anchored_message_) {
    return;
  }
  pmc_timeout_timer_.Stop();
  if (product_messaging_controller_) {
    product_messaging_controller_->UnqueueMessage(kAnchoredMessageId);
  }
  pending_anchored_message_.reset();
  if (active_chips_.empty() && !active_anchored_message_) {
    active_priority_.reset();
  }
}

void PriorityChipSelector::OnPmcPermissionGranted(
    actions::ActionId page_action_id,
    user_education::ProductMessagingHandle handle) {
  if (!pending_anchored_message_ ||
      pending_anchored_message_->page_action_id != page_action_id) {
    return;
  }
  pmc_timeout_timer_.Stop();
  pmc_handle_ = std::move(handle);
  pmc_handle_->SetShown();
  AnchoredMessageConfig config = pending_anchored_message_->config;
  pending_anchored_message_.reset();
  ShowAnchoredMessage(page_action_id, config);
}

void PriorityChipSelector::OnPmcTimeout(actions::ActionId page_action_id) {
  if (!pending_anchored_message_ ||
      pending_anchored_message_->page_action_id != page_action_id) {
    return;
  }
  PageActionPriorityCategory priority =
      pending_anchored_message_->config.priority;
  CancelPendingAnchoredMessage();
  ShowChip(page_action_id, {.priority = priority});
}

}  // namespace internal

std::unique_ptr<ChipSelector> CreateChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId,
                                 const AnchoredMessageConfig&)>
        show_anchored_message_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        hide_anchored_message_callback,
    user_education::ProductMessagingController* product_messaging_controller) {
  if (base::FeatureList::IsEnabled(features::kPageActionsPrioritySelector)) {
    return std::make_unique<internal::PriorityChipSelector>(
        show_chip_callback, hide_chip_callback, show_anchored_message_callback,
        hide_anchored_message_callback, product_messaging_controller);
  }
  return std::make_unique<internal::DefaultChipSelector>(
      show_chip_callback, hide_chip_callback, show_anchored_message_callback,
      hide_anchored_message_callback);
}

}  // namespace page_actions
