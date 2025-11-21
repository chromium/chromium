
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_PAGE_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_PAGE_ACTION_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ScopedWindowCallToAction;

namespace page_actions {
class PageActionController;
}

namespace tabs {
class TabInterface;
}

namespace commerce {

class CommerceUiTabHelper;

// Coordinates the visual state of the Discounts page-action icon/chip
// The business logic lives in CommerceUiTabHelper and related controllers.
class DiscountsPageActionViewController final
    : public page_actions::PageActionObserver {
 public:
  DECLARE_USER_DATA(DiscountsPageActionViewController);

  explicit DiscountsPageActionViewController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller,
      CommerceUiTabHelper& commerce_ui_tab_helper);

  DiscountsPageActionViewController(const DiscountsPageActionViewController&) =
      delete;
  DiscountsPageActionViewController& operator=(
      const DiscountsPageActionViewController&) = delete;

  ~DiscountsPageActionViewController() override;

  static DiscountsPageActionViewController* From(tabs::TabInterface& tab);

  // Updates icon/chip visibility based on the given argument. It do not handle
  // bubble display.
  void UpdatePageIcon(bool should_show_icon, bool should_expand_icon);

  // Attempts to show the discounts bubble when possible. The chip will get
  // collapsed once the bubble is resolved.
  void MaybeShowBubble(bool from_user);

 private:
  // Collapses the suggestion chip if the chip was previously active or will be
  // a noop.
  void HideSuggestionChip();

  // page_actions::PageActonObserver:
  void OnPageActionChipShown(
      const page_actions::PageActionState& page_action) override;

  // Unowned reference to the tab interface that this controller belong to.
  const raw_ref<tabs::TabInterface> tab_interface_;

  // Unowned reference to the page action controller that will coordinate
  // requests from this object.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  // Unowned reference that provide the business logic and set during
  // initialization.
  const raw_ref<CommerceUiTabHelper> commerce_ui_tab_helper_;

  // Keeps the browser-window “call-to-action” highlight alive while the chip
  // label is expanded. Automatically clears when reset or when the controller
  // is destroyed.
  std::unique_ptr<ScopedWindowCallToAction> scoped_window_call_to_action_ptr_;

  ui::ScopedUnownedUserData<DiscountsPageActionViewController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<DiscountsPageActionViewController> weak_ptr_factory_{
      this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_PAGE_ACTION_VIEW_CONTROLLER_H_
