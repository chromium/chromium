// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_icon_view.h"

#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

DiscountsIconView::DiscountsIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "Discounts"),
      bubble_coordinator_(this) {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT));
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kDiscountsChipElementId);
}

DiscountsIconView::~DiscountsIconView() = default;

views::BubbleDialogDelegate* DiscountsIconView::GetBubble() const {
  return bubble_coordinator_.GetBubble();
}

void DiscountsIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  MaybeShowBubble(/*from_user=*/true);

  commerce::CommerceUiTabHelper* tab_helper = GetTabHelper();

  if (!tab_helper) {
    return;
  }

  commerce::metrics::DiscountsMetricCollector::
      RecordDiscountsPageActionIconClicked(
          tab_helper->IsPageActionIconExpanded(PageActionIconType::kDiscounts),
          tab_helper->GetDiscounts());
}

const gfx::VectorIcon& DiscountsIconView::GetVectorIcon() const {
  return vector_icons::kShoppingmodeIcon;
}

void DiscountsIconView::UpdateImpl() {
  bool should_show = ShouldShow();

  if (should_show) {
    MaybeShowPageActionLabel();
  } else {
    HidePageActionLabel();
  }
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  SetVisible(should_show);
}

bool DiscountsIconView::ShouldShow() {
  if (!GetWebContents() || delegate()->ShouldHidePageActionIcons()) {
    return false;
  }

  commerce::CommerceUiTabHelper* tab_helper = GetTabHelper();

  return tab_helper && tab_helper->ShouldShowDiscountsIconView();
}

void DiscountsIconView::MaybeShowPageActionLabel() {
  commerce::CommerceUiTabHelper* tab_helper = GetTabHelper();

  if (!tab_helper ||
      !tab_helper->ShouldExpandPageActionIcon(PageActionIconType::kDiscounts)) {
    return;
  }

  should_extend_label_shown_duration_ = true;
  AnimateIn(IDS_DISCOUNT_ICON_EXPANDED_TEXT);
}

void DiscountsIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

void DiscountsIconView::AnimationProgressed(const gfx::Animation* animation) {
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
        base::BindRepeating(&DiscountsIconView::UnpauseAnimation,
                            base::Unretained(this)));
    MaybeShowBubble(false);
  }
}

commerce::CommerceUiTabHelper* DiscountsIconView::GetTabHelper() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return tabs::TabInterface::GetFromContents(web_contents)
      ->GetTabFeatures()
      ->commerce_ui_tab_helper();
}

void DiscountsIconView::MaybeShowBubble(bool from_user) {
  commerce::CommerceUiTabHelper* tab_helper = GetTabHelper();

  if (!tab_helper) {
    return;
  }

  const std::vector<commerce::DiscountInfo>& discount_infos =
      tab_helper->GetDiscounts();

  CHECK(!discount_infos.empty());

  // Currently only uses the first discount info.
  bool should_auto_show = tab_helper->ShouldAutoShowDiscountsBubble(
      discount_infos[0].id, discount_infos[0].is_merchant_wide);
  if (!from_user && !should_auto_show) {
    return;
  }

  if (animate_out_timer_.IsRunning()) {
    animate_out_timer_.Stop();
  }

  bubble_coordinator_.Show(GetWebContents(), discount_infos[0],
                           base::BindOnce(&DiscountsIconView::UnpauseAnimation,
                                          weak_ptr_factory_.GetWeakPtr()));

  tab_helper->DiscountsBubbleShown(discount_infos[0].id);

  commerce::metrics::DiscountsMetricCollector::RecordDiscountBubbleShown(
      should_auto_show,
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
      tab_helper->GetDiscounts());
}

BEGIN_METADATA(DiscountsIconView)
END_METADATA
