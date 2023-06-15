// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

PriceInsightsIconView::PriceInsightsIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PriceInsights"),
      icon_(OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
                ? &vector_icons::kShoppingBagRefreshIcon
                : &vector_icons::kShoppingBagIcon) {
  SetProperty(views::kElementIdentifierKey, kPriceInsightsChipElementId);
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT));
}
PriceInsightsIconView::~PriceInsightsIconView() = default;

views::BubbleDialogDelegate* PriceInsightsIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& PriceInsightsIconView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? vector_icons::kShoppingBagRefreshIcon
             : vector_icons::kShoppingBagIcon;
}

void PriceInsightsIconView::UpdateImpl() {
  SetVisible(ShouldShow());
}

void PriceInsightsIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);

  tab_helper->ShowShoppingInsightsSidePanel();
}

bool PriceInsightsIconView::ShouldShow() const {
  if (delegate()->ShouldHidePageActionIcons()) {
    return false;
  }
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);

  return tab_helper && tab_helper->ShouldShowPriceInsightsIconView();
}

BEGIN_METADATA(PriceInsightsIconView, PageActionIconView)
END_METADATA
