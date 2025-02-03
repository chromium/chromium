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

PageActionView::PageActionView(actions::ActionItem* action_item,
                               const PageActionViewParams& params)
    : IconLabelBubbleView(gfx::FontList(), params.icon_label_bubble_delegate),
      action_item_(action_item->GetAsWeakPtr()),
      icon_size_(params.icon_size),
      icon_insets_(params.icon_insets) {
  CHECK(action_item_->GetActionId().has_value());

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

  UpdateIconImage();
  UpdateBorder();
  UpdateStyle(model.GetShowSuggestionChip());
}

void PageActionView::UpdateStyle(bool is_suggestion_chip) {
  SetUseTonalColorsWhenExpanded(is_suggestion_chip);
  SetBackgroundVisibility(is_suggestion_chip ? BackgroundVisibility::kAlways
                                             : BackgroundVisibility::kNever);
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

bool PageActionView::ShouldShowLabel() const {
  // TODO(382068900): Update this when the chip with a label state is
  // implemented. In that state, the label should be displayed. However, if
  // there isn't enough space for the label, it should remain hidden.
  return should_show_label_;
}

void PageActionView::SetShouldShowLabelForTesting(bool should_show_label) {
  should_show_label_ = should_show_label;
}

void PageActionView::UpdateBorder() {
  gfx::Insets new_insets = icon_insets_;
  if (ShouldShowLabel()) {
    new_insets += gfx::Insets::TLBR(0, 4, 0, 8);
  }
  if (new_insets != GetInsets()) {
    SetBorder(views::CreateEmptyBorder(new_insets));
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
  if (icon_image.Size() == gfx::Size(icon_size_, icon_size_)) {
    return;
  }

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

BEGIN_METADATA(PageActionView)
END_METADATA

}  // namespace page_actions
