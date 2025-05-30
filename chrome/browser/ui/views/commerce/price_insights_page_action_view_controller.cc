// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_page_action_view_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace commerce {

PriceInsightsPageActionViewController::PriceInsightsPageActionViewController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  CHECK(IsPageActionMigrated(PageActionIconType::kPriceInsights));
}

PriceInsightsPageActionViewController::
    ~PriceInsightsPageActionViewController() = default;

void PriceInsightsPageActionViewController::UpdatePageActionIcon(
    bool should_shown_icon,
    bool should_expand_icon,
    PriceInsightsIconLabelType label_type) {
  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);

  if (!should_shown_icon) {
    // Suggestion chip may be previously shown, ensure that the state is
    // cleared.
    page_action_controller->HideSuggestionChip(kActionCommercePriceInsights);
    page_action_controller->Hide(kActionCommercePriceInsights);
    scoped_window_call_to_action_ptr_.reset();
    return;
  }

  page_action_controller->Show(kActionCommercePriceInsights);

  if (!should_expand_icon) {
    return;
  }

  if (!tab_interface_->GetBrowserWindowInterface()->CanShowCallToAction()) {
    return;
  }

  scoped_window_call_to_action_ptr_ =
      tab_interface_->GetBrowserWindowInterface()->ShowCallToAction();

  switch (label_type) {
    case PriceInsightsIconLabelType::kPriceIsLow:
      page_action_controller->OverrideText(
          kActionCommercePriceInsights,
          l10n_util::GetStringUTF16(
              IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE));
      break;
    case PriceInsightsIconLabelType::kPriceIsHigh:
      page_action_controller->OverrideText(
          kActionCommercePriceInsights,
          l10n_util::GetStringUTF16(
              IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_HIGH_PRICE));
      break;
    default:
      page_action_controller->ClearOverrideText(kActionCommercePriceInsights);
  }

  page_action_controller->ShowSuggestionChip(kActionCommercePriceInsights);
}

}  // namespace commerce
