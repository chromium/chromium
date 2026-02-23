// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_CHIP_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_CHIP_SELECTOR_H_

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/actions/action_id.h"

namespace page_actions {

struct SuggestionChipConfig;

// ChipSelector is an interface for the logic handling showing and hiding of
// suggestion chips and anchored messages.
class ChipSelector {
 public:
  ChipSelector() = default;
  virtual ~ChipSelector() = default;
  virtual void RequestChipShow(actions::ActionId page_action_id,
                               const SuggestionChipConfig& config) = 0;
  virtual void RequestChipHide(actions::ActionId page_action_id) = 0;
  virtual void RequestAnchoredMessageShow(actions::ActionId page_action_id) = 0;
  virtual void RequestAnchoredMessageHide(actions::ActionId page_action_id) = 0;
};

// CreateChipSelector returns the appropriate implementation of the
// ChipSelector. The choice will be controlled by a Finch flag.
std::unique_ptr<ChipSelector> CreateChipSelector(
    base::RepeatingCallback<void(actions::ActionId,
                                 const SuggestionChipConfig&)>
        show_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        show_anchored_message_callback,
    base::RepeatingCallback<void(actions::ActionId)>
        hide_anchored_message_callback);

namespace internal {

// The default implementation of the ChipSelector, which accepts all
// show and hide requests for suggestion chips and implements a FIFO queue for
// anchored messages. Also ensures exactly one of suggestion chip and/or
// anchored message is shown for a given page action.
class DefaultChipSelector : public ChipSelector {
 public:
  DefaultChipSelector(
      base::RepeatingCallback<void(actions::ActionId,
                                   const SuggestionChipConfig&)>
          show_chip_callback,
      base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback,
      base::RepeatingCallback<void(actions::ActionId)>
          show_anchored_message_callback,
      base::RepeatingCallback<void(actions::ActionId)>
          hide_anchored_message_callback);
  ~DefaultChipSelector() override;
  void RequestChipShow(actions::ActionId page_action_id,
                       const SuggestionChipConfig& config) override;
  void RequestChipHide(actions::ActionId page_action_id) override;
  void RequestAnchoredMessageShow(actions::ActionId page_action_id) override;
  void RequestAnchoredMessageHide(actions::ActionId page_action_id) override;

 private:
  const base::RepeatingCallback<void(actions::ActionId,
                                     const SuggestionChipConfig&)>
      show_chip_callback_;
  const base::RepeatingCallback<void(actions::ActionId)> hide_chip_callback_;
  const base::RepeatingCallback<void(actions::ActionId)>
      show_anchored_message_callback_;
  const base::RepeatingCallback<void(actions::ActionId)>
      hide_anchored_message_callback_;
  std::set<actions::ActionId> active_chips_;
  std::vector<actions::ActionId> anchored_message_queue_;
};
}  // namespace internal

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_CHIP_SELECTOR_H_
