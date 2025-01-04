// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_constants.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"

namespace {
bool ShouldShow(base::WeakPtr<actions::ActionItem> action_item,
                page_actions::PageActionModel* model) {
  return action_item.get() && action_item->GetEnabled() &&
         action_item->GetVisible() && model && model->show_requested();
}
}  // namespace

namespace page_actions {

PageActionView::PageActionView(actions::ActionItem* action_item,
                               IconLabelBubbleView::Delegate* parent_delegate)
    : IconLabelBubbleView(gfx::FontList(), parent_delegate),
      action_item_(action_item->GetAsWeakPtr()) {
  CHECK(action_item_->GetActionId().has_value());

  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  UpdateBorder();
}

PageActionView::~PageActionView() = default;

void PageActionView::OnNewActiveController(PageActionController* controller) {
  observation_.Reset();
  if (controller) {
    controller->AddObserver(action_item_->GetActionId().value(), observation_);
  }
  SetVisible(ShouldShow(action_item_, observation_.GetSource()));
}

void PageActionView::OnPageActionModelChanged(PageActionModel* model) {
  SetVisible(ShouldShow(action_item_, model));
  UpdateBorder();
}

void PageActionView::OnPageActionModelWillBeDeleted(PageActionModel* model) {
  observation_.Reset();
  SetVisible(false);
}

std::unique_ptr<views::ActionViewInterface>
PageActionView::GetActionViewInterface() {
  return std::make_unique<PageActionViewInterface>(this,
                                                   observation_.GetSource());
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
  gfx::Insets new_insets =
      GetLayoutInsets(LOCATION_BAR_PAGE_ACTION_ICON_PADDING);
  if (ShouldShowLabel()) {
    new_insets += gfx::Insets::TLBR(0, 4, 0, kPageActionBetweenIconSpacing);
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

void PageActionView::UpdateIconImage() {
  // Icon default size may be different from the size used in the location bar.
  const int icon_size = GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
  const auto icon_image = action_item_->GetImage();
  if (icon_image.Size() == gfx::Size(icon_size, icon_size)) {
    return;
  }

  const gfx::ImageSkia image = gfx::CreateVectorIcon(
      *action_item_->GetImage().GetVectorIcon().vector_icon(), icon_size,
      GetForegroundColor());

  if (!image.isNull()) {
    SetImageModel(ui::ImageModel::FromImageSkia(image));
  }
}

BEGIN_METADATA(PageActionView)
END_METADATA

PageActionViewInterface::PageActionViewInterface(PageActionView* action_view,
                                                 PageActionModel* model)
    : views::LabelButtonActionViewInterface(action_view),
      action_view_(action_view),
      model_(model) {}

PageActionViewInterface::~PageActionViewInterface() = default;

void PageActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  views::LabelButtonActionViewInterface::ActionItemChangedImpl(action_item);
  action_view_->SetVisible(ShouldShow(action_item->GetAsWeakPtr(), model_));
}

}  // namespace page_actions
