// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"

namespace page_actions {

PageActionView::PageActionView(
    actions::ActionItem* action_item,
    const PageActionViewParams& params,
    base::RepeatingCallback<void(actions::ActionId, bool)>
        chip_state_changed_callback)
    : IconLabelBubbleView(gfx::FontList(), params.icon_label_bubble_delegate),
      action_item_(action_item->GetAsWeakPtr()),
      icon_size_(params.icon_size),
      icon_insets_(params.icon_insets),
      chip_state_changed_callback_(chip_state_changed_callback) {
  CHECK(action_item_->GetActionId().has_value());

  if (params.font_list) {
    SetFontList(*params.font_list);
  }

  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  UpdateBorder();
  SetVisible(false);
}

PageActionView::~PageActionView() = default;

void PageActionView::OnNewActiveController(PageActionController* controller) {
  observation_.Reset();
  action_item_controller_subscription_ = {};
  if (controller) {
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
  SetText(model.GetText());
  SetTooltipText(model.GetTooltipText());
  label()->SetVisible(model.GetShowSuggestionChip());

  UpdateIconImage();
  UpdateBorder();
  UpdateStyle(model.GetShowSuggestionChip());
}

void PageActionView::UpdateStyle(bool is_suggestion_chip) {
  SetUseTonalColorsWhenExpanded(is_suggestion_chip);
  SetBackgroundVisibility(is_suggestion_chip ? BackgroundVisibility::kAlways
                                             : BackgroundVisibility::kNever);

  // Only trigger the chip state changed callback if there's an actual change
  // in the suggestion chip's visibility. This prevents unnecessary updates and
  // reordering logic in the container when the chip state remains unchanged.
  if (showing_suggestion_chip_ != is_suggestion_chip) {
    showing_suggestion_chip_ = is_suggestion_chip;
    chip_state_changed_callback_.Run(GetActionId(), showing_suggestion_chip_);
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
    UpdateBorder();
  }
}

void PageActionView::UpdateBorder() {
  gfx::Insets insets = icon_insets_;
  if (ShouldShowLabel()) {
    constexpr int kInsetsLeftPadding = 4;
    constexpr int kInsetsRightPadding = 8;
    insets +=
        gfx::Insets().set_left_right(kInsetsLeftPadding, kInsetsRightPadding);
  }

  if (GetInsets() != insets) {
    SetBorder(views::CreateEmptyBorder(insets));
  }
}

bool PageActionView::ShouldShowSeparator() const {
  return false;
}

bool PageActionView::ShouldUpdateInkDropOnClickCanceled() const {
  return true;
}

void PageActionView::NotifyClick(const ui::Event& event) {
  IconLabelBubbleView::NotifyClick(event);

  if (skip_action_invocation_) {
    skip_action_invocation_ = false;
    return;
  }

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
}

void PageActionView::UpdateIconImage() {
  if (observation_.GetSource() == nullptr ||
      observation_.GetSource()->GetImage().IsEmpty()) {
    return;
  }

  // Icon default size may be different from the size used in the location bar.
  const auto& icon_image = observation_.GetSource()->GetImage();
  const gfx::ImageSkia image =
      gfx::CreateVectorIcon(*icon_image.GetVectorIcon().vector_icon(),
                            icon_size_, GetForegroundColor());

  if (!image.isNull()) {
    SetImageModel(ui::ImageModel::FromImageSkia(image));
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

bool PageActionView::OnMousePressed(const ui::MouseEvent& event) {
  // If an action is already displaying a bubble, don't reinvoke the action on
  // the pending click (the click should hide the bubble, but not spawn a
  // new one). This state must be cleared on NotifyClick() or OnClickCanceled(),
  // otherwise it will persist and affect future non-mouse input.
  // An alternative to this approach is to intercept and conditionally not
  // propagate OnMouseReleased, thus not triggering NotifyClick().
  if (observation_.IsObserving()) {
    skip_action_invocation_ =
        observation_.GetSource()->GetActionItemIsShowingBubble();
  }
  return IconLabelBubbleView::OnMousePressed(event);
}

void PageActionView::OnClickCanceled(const ui::Event& event) {
  skip_action_invocation_ = false;
}

views::View* PageActionView::GetLabelForTesting() {
  return label();
}

BEGIN_METADATA(PageActionView)
END_METADATA

}  // namespace page_actions
