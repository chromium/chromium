// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_VIEW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"

class ScopedWindowCallToAction;

namespace page_actions {
class PageActionController;
}

namespace tabs {
class TabInterface;
}

namespace commerce {
class CommerceUiTabHelper;
}

namespace commerce {

// This class is responsible for interacting with the PageActionController
// and CommerceUITabHelper to determine whether the Product Specification icon
// should be shown, hidden, or expanded with additional text based on the
// current page's commerce-related context.
class ProductSpecificationsPageActionViewController {
 public:
  ProductSpecificationsPageActionViewController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller,
      CommerceUiTabHelper& commerce_ui_tab_helper);
  ~ProductSpecificationsPageActionViewController();

  ProductSpecificationsPageActionViewController(
      const ProductSpecificationsPageActionViewController&) = delete;
  ProductSpecificationsPageActionViewController& operator=(
      const ProductSpecificationsPageActionViewController&) = delete;

  // Show/hide the icon + chip.
  void UpdatePageIcon(bool should_show_icon,
                      bool should_expand_icon,
                      bool is_in_recommendation_set,
                      const std::u16string& label);

  // Fires the “added to comparison” toast.
  void ShowConfirmationToast();

 private:
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
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_VIEW_CONTROLLER_H_
