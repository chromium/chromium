// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/bottom_container_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/background.h"
#include "ui/views/layout/layout_provider.h"

namespace {
class BottomContainerButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit BottomContainerButtonActionViewInterface(
      BottomContainerButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}
  ~BottomContainerButtonActionViewInterface() override = default;

  // views::LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    // Calling ButtonActionViewInterface instead of
    // LabelButtonActionViewInterface to avoid the text of the button being set.
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetImageModel(action_view_->GetState(),
                                action_item->GetImage());
  }

 private:
  raw_ptr<BottomContainerButton> action_view_;
};
}  // namespace

BottomContainerButton::BottomContainerButton() {
  SetBackground(views::CreateRoundedRectBackground(
      kColorVerticalTabStripBottomButtonBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)));
}

std::unique_ptr<views::ActionViewInterface>
BottomContainerButton::GetActionViewInterface() {
  return std::make_unique<BottomContainerButtonActionViewInterface>(this);
}

BEGIN_METADATA(BottomContainerButton)
END_METADATA
