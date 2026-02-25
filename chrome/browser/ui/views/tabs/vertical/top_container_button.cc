// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_interface.h"

namespace {
class TopContainerButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit TopContainerButtonActionViewInterface(
      TopContainerButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

  void OnViewChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::OnViewChangedImpl(action_item);
    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

 private:
  raw_ptr<TopContainerButton> action_view_ = nullptr;
};
}  // namespace

TopContainerButton::TopContainerButton() {
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  ConfigureInkDropForToolbar(this);
}

void TopContainerButton::UpdateIcon(const ui::ImageModel& icon_image) {
  CHECK(icon_image.IsVectorIcon());

  const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
      *icon_image.GetVectorIcon().vector_icon(), GetForegroundColor(),
      GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonIconSize));

  SetImageModel(views::Button::STATE_NORMAL, image_model);
  SetImageModel(views::Button::STATE_HOVERED, image_model);
  SetImageModel(views::Button::STATE_PRESSED, image_model);
  SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void TopContainerButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &View::NotifyViewControllerCallback, base::Unretained(this)));
}

void TopContainerButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

ui::ColorId TopContainerButton::GetForegroundColor() const {
  return GetWidget() && GetWidget()->ShouldPaintAsActive()
             ? kColorTabForegroundInactiveFrameActive
             : kColorTabForegroundInactiveFrameInactive;
}

std::unique_ptr<views::ActionViewInterface>
TopContainerButton::GetActionViewInterface() {
  return std::make_unique<TopContainerButtonActionViewInterface>(this);
}

BEGIN_METADATA(TopContainerButton)
END_METADATA
