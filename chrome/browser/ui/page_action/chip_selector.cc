// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/chip_selector.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/actions/action_id.h"

namespace page_actions {

namespace internal {
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

PriorityChipSelector::PriorityChipSelector(
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

PriorityChipSelector::~PriorityChipSelector() = default;

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
  if (active_chips_.empty() && !active_anchored_message_) {
    active_priority_.reset();
  }
}

void PriorityChipSelector::RequestAnchoredMessageShow(
    actions::ActionId page_action_id,
    const AnchoredMessageConfig& config) {
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
  if (active_chips_.contains(page_action_id)) {
    // This page action is currently showing a suggestion chip. Hide it and then
    // attempt to show the anchored message.
    RequestChipHide(page_action_id);
  }

  if (!active_priority_) {
    // No active suggestion chip or anchored message, so we show the request.
    ShowAnchoredMessage(page_action_id, config);
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
    ShowAnchoredMessage(page_action_id, config);
    return;
  } else if (config.priority == PageActionPriorityCategory::kUserInteraction) {
    // User interaction -> downgrade visible anchored message (if any) to a
    // suggestion chip, and show the requested one.
    if (active_anchored_message_) {
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
  // Privacy/Security. If we are already showing an anchored message, the new
  // request is downgraded to a suggestion chip, otherwise, we show it.
  if (active_anchored_message_) {
    ShowChip(page_action_id,
             SuggestionChipConfig{
                 .priority = PageActionPriorityCategory::kPrivacySecurity});
  } else {
    ShowAnchoredMessage(page_action_id, config);
  }
}

void PriorityChipSelector::RequestAnchoredMessageHide(
    actions::ActionId page_action_id) {
  if (active_anchored_message_ != page_action_id) {
    return;
  }
  active_anchored_message_.reset();
  hide_anchored_message_callback_.Run(page_action_id);

  if (active_chips_.empty()) {
    active_priority_.reset();
  }
}

void PriorityChipSelector::HideAllActive() {
  for (const auto chip_id : active_chips_) {
    hide_chip_callback_.Run(chip_id);
  }
  active_chips_.clear();
  if (active_anchored_message_) {
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
        hide_anchored_message_callback) {
  if (base::FeatureList::IsEnabled(features::kPageActionsPrioritySelector)) {
    return std::make_unique<internal::PriorityChipSelector>(
        show_chip_callback, hide_chip_callback, show_anchored_message_callback,
        hide_anchored_message_callback);
  }
  return std::make_unique<internal::DefaultChipSelector>(
      show_chip_callback, hide_chip_callback, show_anchored_message_callback,
      hide_anchored_message_callback);
}

}  // namespace page_actions
