// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"

#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/discounts_page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace commerce {

DEFINE_USER_DATA(DiscountsPageActionViewController);

DiscountsPageActionViewController::DiscountsPageActionViewController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller,
    CommerceUiTabHelper& commerce_ui_tab_helper)
    : PageActionObserver(kActionCommerceDiscounts),
      tab_interface_(tab_interface),
      page_action_controller_(page_action_controller),
      commerce_ui_tab_helper_(commerce_ui_tab_helper),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  CHECK(IsPageActionMigrated(PageActionIconType::kDiscounts));
  RegisterAsPageActionObserver(*page_action_controller_);
}

DiscountsPageActionViewController::~DiscountsPageActionViewController() =
    default;

// static
DiscountsPageActionViewController* DiscountsPageActionViewController::From(
    tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void DiscountsPageActionViewController::UpdatePageIcon(
    bool should_show_icon,
    bool should_expand_icon) {
  if (!should_show_icon) {
    page_action_controller_->HideSuggestionChip(kActionCommerceDiscounts);
    page_action_controller_->Hide(kActionCommerceDiscounts);

    scoped_window_call_to_action_ptr_.reset();
    return;
  }

  if (!tab_interface_->GetBrowserWindowInterface()->CanShowCallToAction()) {
    return;
  }

  scoped_window_call_to_action_ptr_ =
      tab_interface_->GetBrowserWindowInterface()->ShowCallToAction();

  page_action_controller_->Show(kActionCommerceDiscounts);

  if (!should_expand_icon) {
    page_action_controller_->HideSuggestionChip(kActionCommerceDiscounts);
    return;
  }

  // Show the suggestion chip. Timing / animation is handled by
  // PageActionController internally.
  page_action_controller_->ShowSuggestionChip(kActionCommerceDiscounts,
                                              {.should_animate = true});
}

void DiscountsPageActionViewController::MaybeShowBubble(bool from_user) {
  const std::vector<commerce::DiscountInfo>& discount_infos =
      commerce_ui_tab_helper_->GetDiscounts();
  CHECK(!discount_infos.empty());

  // Currently only uses the first discount info.
  bool should_auto_show =
      commerce_ui_tab_helper_->ShouldAutoShowDiscountsBubble(
          discount_infos[0].id, discount_infos[0].is_merchant_wide);
  if (!from_user && !should_auto_show) {
    return;
  }

  auto* web_contents = tab_interface_->GetContents();
  CHECK(web_contents);

  if (!from_user && should_auto_show) {
    // If commerce::kDiscountDialogAutoPopupCounterfactual is enabled, we
    // purposely not show the bubble.
    bool should_suppress = base::FeatureList::IsEnabled(
        commerce::kDiscountDialogAutoPopupCounterfactual);
    commerce::metrics::DiscountsMetricCollector::
        RecordDiscountAutoPopupEligibleButSuppressed(
            web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            should_suppress);
    if (should_suppress) {
      return;
    }
  }

  commerce_ui_tab_helper_->ShowDiscountBubble(
      discount_infos[0],
      base::BindOnce(&DiscountsPageActionViewController::HideSuggestionChip,
                     weak_ptr_factory_.GetWeakPtr()));
  commerce_ui_tab_helper_->DiscountsBubbleShown(discount_infos[0].id);

  commerce::metrics::DiscountsMetricCollector::RecordDiscountBubbleShown(
      should_auto_show,
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
      commerce_ui_tab_helper_->GetDiscounts());
}

void DiscountsPageActionViewController::HideSuggestionChip() {
  page_action_controller_->HideSuggestionChip(kActionCommerceDiscounts);
}

void DiscountsPageActionViewController::OnPageActionChipShown(
    const page_actions::PageActionState& page_action) {
  MaybeShowBubble(/*from_user=*/false);
}

}  // namespace commerce
