// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_icon_view.h"

#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
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
                         "Discounts") {
  GetViewAccessibility().SetProperties(
      /*role*/ std::nullopt,
      l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT));
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kDiscountsChipElementId);
}

DiscountsIconView::~DiscountsIconView() = default;

views::BubbleDialogDelegate* DiscountsIconView::GetBubble() const {
  // TODO(b/351935350): return discount bubble.
  NOTIMPLEMENTED();
  return nullptr;
}

void DiscountsIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  // TODO(b/351935350): Open discount bubble.
  NOTIMPLEMENTED();
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
  }
}

commerce::CommerceUiTabHelper* DiscountsIconView::GetTabHelper() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return commerce::CommerceUiTabHelper::FromWebContents(web_contents);
}

BEGIN_METADATA(DiscountsIconView)
END_METADATA
