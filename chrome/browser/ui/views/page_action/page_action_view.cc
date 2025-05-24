// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

PageActionView::PageActionView(actions::ActionItem* action_item,
                               const PageActionViewParams& params,
                               ui::ElementIdentifier element_identifier)
    : IconLabelBubbleView(gfx::FontList(), params.icon_label_bubble_delegate),
      action_item_(action_item->GetAsWeakPtr()),
      icon_size_(params.icon_size),
      icon_insets_(params.icon_insets) {
  CHECK(action_item_->GetActionId().has_value());
  SetUpForAnimation(base::Milliseconds(600));

  SetProperty(views::kElementIdentifierKey, element_identifier);

  if (params.font_list) {
    SetFontList(*params.font_list);
  }

  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetVisible(false);
  label()->SetVisible(false);
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  UpdateBorder();
  SetExpandedLabelAdditionalInsets(views::Inset1D(4, 8));

  label_visibility_changed_subscription_ =
      label()->AddVisibleChangedCallback(base::BindRepeating(
          &PageActionView::OnLabelVisibilityChanged, base::Unretained(this)));
}

PageActionView::~PageActionView() = default;

bool PageActionView::IsChipVisible() const {
  return ShouldShowLabel();
}

base::CallbackListSubscription PageActionView::AddChipVisibilityChangedCallback(
    ChipVisibilityChanged callback) {
  return chip_visibility_changed_callbacks_.Add(std::move(callback));
}

void PageActionView::OnNewActiveController(PageActionController* controller) {
  observation_.Reset();
  action_item_controller_subscription_ = {};
  if (controller) {
    click_callback_ =
        controller->GetClickCallback(action_item_->GetActionId().value());
    controller->AddObserver(action_item_->GetActionId().value(), observation_);
    // TODO(crbug.com/388524315): Have the controller manage its own ActionItem
    // observation. See bug for more explanation.
    action_item_controller_subscription_ =
        controller->CreateActionItemSubscription(action_item_.get());
    OnPageActionModelChanged(*observation_.GetSource());
  } else {
    SetVisible(false);
  }
}

void PageActionView::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  SetEnabled(model.GetVisible());
  SetVisible(model.GetVisible());
  SetLabel(model.GetText(), model.GetAccessibleName());
  SetTooltipText(model.GetTooltipText());
  UpdateIconImage();

  const bool was_chip_visible = IsChipVisible();
  if (!model.GetVisible()) {
    ResetSlideAnimation(/*show=*/false);
  } else if (!model.GetShouldAnimateChip()) {
    ResetSlideAnimation(/*show=*/model.GetShowSuggestionChip());
  } else if (model.GetShowSuggestionChip()) {
    AnimateIn(/*string_id=*/std::nullopt);
  } else {
    AnimateOut();
  }

  // Announce the chip only if announcements are enabled and the chip was
  // newly shown.
  if (model.GetShouldAnnounceChip() && !was_chip_visible && IsChipVisible()) {
    GetViewAccessibility().AnnounceAlert(label()->GetText());
  }
}

void PageActionView::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  observation_.Reset();
  action_item_controller_subscription_ = {};
  SetVisible(false);
}

actions::ActionId PageActionView::GetActionId() const {
  return action_item_->GetActionId().value();
}

void PageActionView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIconImage();
}

void PageActionView::OnTouchUiChanged() {
  IconLabelBubbleView::OnTouchUiChanged();
  UpdateIconImage();
}

void PageActionView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this) {
    UpdateIconImage();
  }
}

void PageActionView::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(icon_insets_));
}

bool PageActionView::ShouldShowSeparator() const {
  return false;
}

bool PageActionView::ShouldShowLabelAfterAnimation() const {
  return ShouldShowLabel();
}

bool PageActionView::ShouldUpdateInkDropOnClickCanceled() const {
  return true;
}

void PageActionView::NotifyClick(const ui::Event& event) {
  IconLabelBubbleView::NotifyClick(event);

  PageActionTrigger trigger_source;
  if (event.IsMouseEvent()) {
    trigger_source = PageActionTrigger::kMouse;
  } else if (event.IsKeyEvent()) {
    trigger_source = PageActionTrigger::kKeyboard;
  } else {
    CHECK(event.IsGestureEvent());
    trigger_source = PageActionTrigger::kGesture;
  }
  action_item_->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(kPageActionTriggerKey,
                       static_cast<std::underlying_type_t<PageActionTrigger>>(
                           trigger_source))
          .Build());

  CHECK(click_callback_);
  click_callback_.Run(trigger_source);
}

void PageActionView::UpdateIconImage() {
  if (observation_.GetSource() == nullptr ||
      observation_.GetSource()->GetImage().IsEmpty()) {
    return;
  }
  const auto& icon_image = observation_.GetSource()->GetImage();
  // If image does not have a vector icon, set it directly.
  if (icon_image.IsVectorIcon()) {
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(*icon_image.GetVectorIcon().vector_icon(),
                              icon_size_, GetForegroundColor());

    if (!image.isNull()) {
      SetImageModel(ui::ImageModel::FromImageSkia(image));
    }
  } else {
    SetImageModel(icon_image);
  }
}

void PageActionView::SetModel(PageActionModelInterface* model) {
  observation_.Reset();
  observation_.Observe(model);
}

gfx::Size PageActionView::GetMinimumSize() const {
  gfx::Size icon_preferred_size = image_container_view()->GetPreferredSize();
  icon_preferred_size.Enlarge(icon_insets_.width(), icon_insets_.height());

  return icon_preferred_size;
}

bool PageActionView::IsBubbleShowing() const {
  return observation_.IsObserving() &&
         observation_.GetSource()->GetActionItemIsShowingBubble();
}

bool PageActionView::IsTriggerableEvent(const ui::Event& event) {
  // Returns whether the bubble should be shown given the event. Only trigger an
  // action when action UI isn't already showing (managed at the
  // IconLabelBubbleView level), and if mouse input, when event is a left button
  // click.
  if (event.IsMouseEvent()) {
    // IconLabelBubbleView allows any mouse click to be triggerable event so
    // need to manually check here.
    return IconLabelBubbleView::IsTriggerableEvent(event) &&
           ((GetTriggerableEventFlags() & event.flags()) != 0);
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

void PageActionView::OnLabelVisibilityChanged() {
  UpdateBackground();
  UpdateLabelColors();
  UpdateIconImage();
  chip_visibility_changed_callbacks_.Notify(this);
}

views::View* PageActionView::GetLabelForTesting() {
  return label();
}

gfx::SlideAnimation& PageActionView::GetSlideAnimationForTesting() {
  return slide_animation_;
}

BEGIN_METADATA(PageActionView)
END_METADATA

}  // namespace page_actions
