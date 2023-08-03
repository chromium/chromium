// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view_class_properties.h"

PriceInsightsIconView::PriceInsightsIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Profile* profile)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PriceInsights"),
      profile_(profile),
      icon_(OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
                ? &vector_icons::kShoppingBagRefreshIcon
                : &vector_icons::kShoppingBagIcon) {
  SetUpForInOutAnimation();
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
  bool should_show = ShouldShow();

  if (should_show) {
    if (!GetVisible()) {
      // Reset the last_shown_label_type_ first.
      last_shown_label_type_ =
          PriceInsightsIconView::PriceInsightsIconLabelType::kNone;

      auto label_to_be_shown = GetPriceInsightsIconLabelType();
      if (label_to_be_shown !=
              PriceInsightsIconView::PriceInsightsIconLabelType::kNone &&
          MaybeShowPageActionLabel()) {
        last_shown_label_type_ = label_to_be_shown;
      }
      base::UmaHistogramEnumeration(
          "Commerce.PriceInsights.OmniboxIconShownLabel",
          last_shown_label_type_);
    }
  } else {
    HidePageActionLabel();
  }

  SetVisible(should_show);
}

void PriceInsightsIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

bool PriceInsightsIconView::MaybeShowPageActionLabel() {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);

  if (!tracker ||
      !tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature)) {
    return false;
  }

  should_extend_label_shown_duration_ = true;
  AnimateIn(absl::nullopt);

  // Note that `Dismiss()` in this case does not dismiss the UI. It's telling
  // the FE backend that the promo is done so that other promos can run. Showing
  // the label should not block other promos from displaying.
  tracker->Dismissed(
      feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature);

  return true;
}

PriceInsightsIconView::PriceInsightsIconLabelType
PriceInsightsIconView::GetPriceInsightsIconLabelType() {
  auto* web_contents = GetWebContents();

  if (!web_contents) {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kNone;
  }
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);

  auto& price_insights_info = tab_helper->GetPriceInsightsInfo();

  if (!price_insights_info.has_value() ||
      !price_insights_info->typical_low_price_micros.has_value() ||
      !price_insights_info->typical_high_price_micros.has_value() ||
      price_insights_info->catalog_history_prices.empty()) {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kNone;
  } else if (price_insights_info->price_bucket ==
             commerce::PriceBucket::kLowPrice) {
    SetLabel(l10n_util::GetStringUTF16(
        IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE));
    return PriceInsightsIconView::PriceInsightsIconLabelType::kPriceIsLow;
  } else if (price_insights_info->price_bucket ==
                 commerce::PriceBucket::kHighPrice &&
             commerce::kPriceInsightsChipLabelExpandOnHighPrice.Get()) {
    SetLabel(l10n_util::GetStringUTF16(
        IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_HIGH_PRICE));
    return PriceInsightsIconView::PriceInsightsIconLabelType::kPriceIsHigh;
  } else {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kNone;
  }
}

void PriceInsightsIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/1314206): This approach of inspecting the animation progress
  // to extend the animation duration is quite hacky. This should be removed and
  // the IconLabelBubbleView API expanded to support a finer level of control.
  constexpr double kAnimationValueWhenLabelFullyShown = 0.5;
  constexpr base::TimeDelta kLabelPersistDuration = base::Seconds(10.8);
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();
    animate_out_timer_.Start(
        FROM_HERE, kLabelPersistDuration,
        base::BindRepeating(&PriceInsightsIconView::UnpauseAnimation,
                            base::Unretained(this)));
  }
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

  tab_helper->OnPriceInsightsIconClicked();
  base::UmaHistogramEnumeration(
      "Commerce.PriceInsights.OmniboxIconClickedAfterLabelShown",
      last_shown_label_type_);
  SetHighlighted(false);
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

const std::u16string& PriceInsightsIconView::GetIconLabelForTesting() {
  return label()->GetText();
}

bool PriceInsightsIconView::IsIconHighlightedForTesting() {
  return views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
         views::InkDropState::ACTIVATED;
}

BEGIN_METADATA(PriceInsightsIconView, PageActionIconView)
END_METADATA
