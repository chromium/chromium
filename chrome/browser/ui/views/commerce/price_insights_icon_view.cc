// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
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
      profile_(profile) {
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kPriceInsightsChipElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT));

  if (base::FeatureList::IsEnabled(commerce::kShoppingIconColorVariant)) {
    SetCustomForegroundColorId(kColorShoppingPageActionIconForegroundVariant);
    SetCustomBackgroundColorId(kColorShoppingPageActionIconBackgroundVariant);
  }
}
PriceInsightsIconView::~PriceInsightsIconView() = default;

views::BubbleDialogDelegate* PriceInsightsIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& PriceInsightsIconView::GetVectorIcon() const {
  return vector_icons::kShoppingBagRefreshIcon;
}

void PriceInsightsIconView::UpdateImpl() {
  bool should_show = ShouldShow();

  if (should_show) {
    MaybeShowPageActionLabel();
  } else {
    HidePageActionLabel();
  }
  UpdateBackground();

  SetVisible(should_show);
}

void PriceInsightsIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

void PriceInsightsIconView::MaybeShowPageActionLabel() {
  if (!base::FeatureList::IsEnabled(commerce::kCommerceAllowChipExpansion)) {
    return;
  }
  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  if (!tab_helper || !tab_helper->ShouldExpandPageActionIcon(
                         PageActionIconType::kPriceInsights)) {
    return;
  }

  should_extend_label_shown_duration_ = true;
  UpdatePriceInsightsIconLabel();

  AnimateIn(std::nullopt);
}

PriceInsightsIconLabelType PriceInsightsIconView::GetLabelTypeForPage() {
  auto* web_contents = GetWebContents();

  if (!web_contents) {
    return PriceInsightsIconLabelType::kNone;
  }
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  return tab_helper->GetPriceInsightsIconLabelTypeForPage();
}

void PriceInsightsIconView::UpdatePriceInsightsIconLabel() {
  PriceInsightsIconLabelType label_type = GetLabelTypeForPage();
  if (label_type == PriceInsightsIconLabelType::kPriceIsLow) {
    SetLabel(
        l10n_util::GetStringUTF16(
            IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_LOW_PRICE),
        l10n_util::GetStringUTF16(IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT));
  } else if (label_type == PriceInsightsIconLabelType::kPriceIsHigh) {
    SetLabel(
        l10n_util::GetStringUTF16(
            IDS_SHOPPING_INSIGHTS_ICON_EXPANDED_TEXT_HIGH_PRICE),
        l10n_util::GetStringUTF16(IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT));
  } else {
    SetLabel(u"", l10n_util::GetStringUTF16(
                      IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT));
  }
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
}

void PriceInsightsIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/40832707): This approach of inspecting the animation
  // progress to extend the animation duration is quite hacky. This should be
  // removed and the IconLabelBubbleView API expanded to support a finer level
  // of control.
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
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  tab_helper->OnPriceInsightsIconClicked();
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
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  return tab_helper && tab_helper->ShouldShowPriceInsightsIconView();
}

const std::u16string& PriceInsightsIconView::GetIconLabelForTesting() {
  return label()->GetText();
}

bool PriceInsightsIconView::IsIconHighlightedForTesting() {
  return views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
         views::InkDropState::ACTIVATED;
}

BEGIN_METADATA(PriceInsightsIconView)
END_METADATA
