// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/chip_selector.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "ui/actions/action_id.h"

namespace page_actions {

namespace internal {
DefaultChipSelector::DefaultChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)>
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
    actions::ActionId page_action_id) {
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
  show_anchored_message_callback_.Run(page_action_id);
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
    show_anchored_message_callback_.Run(anchored_message_queue_[0]);
  }
}
}  // namespace internal

std::unique_ptr<ChipSelector> CreateChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        show_anchored_message_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        hide_anchored_message_callback) {
  return std::make_unique<internal::DefaultChipSelector>(
      show_chip_callback, hide_chip_callback, show_anchored_message_callback,
      hide_anchored_message_callback);
}

}  // namespace page_actions
