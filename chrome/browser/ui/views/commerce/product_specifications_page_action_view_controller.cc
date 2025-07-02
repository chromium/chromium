// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_page_action_view_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/ui_utils.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/tabs/public/tab_interface.h"

namespace commerce {

ProductSpecificationsPageActionViewController::
    ProductSpecificationsPageActionViewController(
        tabs::TabInterface& tab_interface,
        page_actions::PageActionController& page_action_controller,
        CommerceUiTabHelper& commerce_ui_tab_helper)
    : tab_interface_(tab_interface),
      page_action_controller_(page_action_controller),
      commerce_ui_tab_helper_(commerce_ui_tab_helper) {
  CHECK(IsPageActionMigrated(PageActionIconType::kProductSpecifications));
}

ProductSpecificationsPageActionViewController::
    ~ProductSpecificationsPageActionViewController() = default;

void ProductSpecificationsPageActionViewController::UpdatePageIcon(
    bool should_show_icon,
    bool should_expand_icon,
    bool is_in_recommendation_set,
    const std::u16string& label) {
  if (!should_show_icon) {
    page_action_controller_->HideSuggestionChip(
        kActionCommerceProductSpecifications);
    page_action_controller_->Hide(kActionCommerceProductSpecifications);
    scoped_window_call_to_action_ptr_.reset();
    return;
  }

  if (!tab_interface_->GetBrowserWindowInterface()->CanShowCallToAction()) {
    return;
  }

  scoped_window_call_to_action_ptr_ =
      tab_interface_->GetBrowserWindowInterface()->ShowCallToAction();

  page_action_controller_->OverrideImage(
      kActionCommerceProductSpecifications,
      is_in_recommendation_set ? ui::ImageModel::FromVectorIcon(
                                     omnibox::kProductSpecificationsAddedIcon)
                               : ui::ImageModel::FromVectorIcon(
                                     omnibox::kProductSpecificationsAddIcon));

  page_action_controller_->Show(kActionCommerceProductSpecifications);

  if (!should_expand_icon) {
    page_action_controller_->HideSuggestionChip(
        kActionCommerceProductSpecifications);
    return;
  }

  page_action_controller_->OverrideText(kActionCommerceProductSpecifications,
                                        label);
  page_action_controller_->ShowSuggestionChip(
      kActionCommerceProductSpecifications, {.should_animate = true});
}

void ProductSpecificationsPageActionViewController::ShowConfirmationToast() {
  commerce_ui_tab_helper_->OnProductSpecificationsIconClicked();
  ShowProductSpecsConfirmationToast(
      commerce_ui_tab_helper_->GetComparisonSetName(),
      tab_interface_->GetBrowserWindowInterface()
          ->GetFeatures()
          .toast_controller());
}

}  // namespace commerce
