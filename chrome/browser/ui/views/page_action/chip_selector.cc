// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/chip_selector.h"

#include <memory>

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
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback)
    : show_chip_callback_(show_chip_callback),
      hide_chip_callback_(hide_chip_callback) {}

DefaultChipSelector::~DefaultChipSelector() = default;

void DefaultChipSelector::RequestChipShow(actions::ActionId page_action_id,
                                          const SuggestionChipConfig& config) {
  if (!active_chips_.contains(page_action_id)) {
    active_chips_.insert(page_action_id);
    base::UmaHistogramExactLinear("PageActionController.ActiveSuggestionChips",
                                  active_chips_.size(), 25);
  }
  show_chip_callback_.Run(page_action_id, config);
}
void DefaultChipSelector::RequestChipHide(actions::ActionId page_action_id) {
  active_chips_.erase(page_action_id);
  hide_chip_callback_.Run(page_action_id);
}
}  // namespace internal

std::unique_ptr<ChipSelector> CreateChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback) {
  return std::make_unique<internal::DefaultChipSelector>(show_chip_callback,
                                                         hide_chip_callback);
}

}  // namespace page_actions
