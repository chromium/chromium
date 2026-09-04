// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_block_button.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

ActionAppMenuBlockButton::ActionAppMenuBlockButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  const auto* provider = ChromeLayoutProvider::Get();
  const int width =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_WIDTH);
  const int height =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_HEIGHT);
  const int icon_size = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_ICON_SIZE);
  const int between_spacing = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_BETWEEN_CHILD_SPACING);
  const int corner_radius = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_CORNER_RADIUS);

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_ENTRY_BUTTON),
      between_spacing);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  SetPreferredSize(gfx::Size(width, height));
  SetBorder(views::CreateRoundedRectBorder(1, corner_radius,
                                           kColorAppMenuBlockButtonBorder));

  // Enable keyboard navigation and focus highlighting.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->GetViewAccessibility().SetIsIgnored(true);
  icon_view_->SetProperty(views::kSkipAccessibilityPaintChecks, true);

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetEnabledColor(kColorAppMenuBlockButtonForeground);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetTextStyle(views::style::STYLE_BODY_5);
  label_->SetElideBehavior(gfx::ELIDE_TAIL);
  label_->GetViewAccessibility().SetIsIgnored(true);
  label_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
}

ActionAppMenuBlockButton::~ActionAppMenuBlockButton() = default;

void ActionAppMenuBlockButton::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  if (GetState() == ButtonState::STATE_HOVERED ||
      GetState() == ButtonState::STATE_PRESSED || HasFocus()) {
    flags.setColor(GetColorProvider()->GetColor(
        kColorAppMenuBlockButtonBackgroundHovered));
  } else {
    flags.setColor(
        GetColorProvider()->GetColor(kColorAppMenuBlockButtonBackground));
  }
  const float corner_radius = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_CORNER_RADIUS);
  canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), corner_radius, flags);
}

void ActionAppMenuBlockButton::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  SchedulePaint();
}

void ActionAppMenuBlockButton::OnFocus() {
  views::Button::OnFocus();
  SchedulePaint();
}

void ActionAppMenuBlockButton::OnBlur() {
  views::Button::OnBlur();
  SchedulePaint();
}

void ActionAppMenuBlockButton::SetText(std::u16string_view text) {
  label_->SetText(std::u16string(text));
  if (!text.empty()) {
    GetViewAccessibility().SetName(std::u16string(text));
    SetTooltipText(std::u16string(text));
  }
}

void ActionAppMenuBlockButton::SetImageModel(
    const ui::ImageModel& image_model) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_ICON_SIZE);
  if (image_model.IsVectorIcon()) {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *image_model.GetVectorIcon().vector_icon(),
        kColorAppMenuBlockButtonForeground, icon_size));
  } else {
    icon_view_->SetImage(image_model);
  }
}

class ActionAppMenuBlockButtonActionViewInterface
    : public views::ButtonActionViewInterface {
 public:
  explicit ActionAppMenuBlockButtonActionViewInterface(
      ActionAppMenuBlockButton* action_view)
      : views::ButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    views::ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    const std::u16string* short_text =
        action_item->GetProperty(actions::kShortTitleTextKey);
    if (short_text && !short_text->empty()) {
      action_view_->SetText(*short_text);
    } else {
      action_view_->SetText(action_item->GetText());
    }
    if (!action_item->GetImage().IsEmpty()) {
      action_view_->SetImageModel(action_item->GetImage());
    }
  }

 private:
  raw_ptr<ActionAppMenuBlockButton> action_view_;
};

std::unique_ptr<views::ActionViewInterface>
ActionAppMenuBlockButton::GetActionViewInterface() {
  return std::make_unique<ActionAppMenuBlockButtonActionViewInterface>(this);
}

BEGIN_METADATA(ActionAppMenuBlockButton)
END_METADATA
