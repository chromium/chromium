// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_button.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
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

ActionAppMenuFooterButton::ActionAppMenuFooterButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  const auto* provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  const int between_spacing = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_BETWEEN_CHILD_SPACING);

  // Arrange the button's icon, label, and optional submenu arrow in a row
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_BUTTON),
      between_spacing);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  // Enable keyboard navigation and focus highlighting.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  // Icon: hidden by default until populated with an ImageModel.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->GetViewAccessibility().SetIsIgnored(true);
  icon_view_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  icon_view_->SetVisible(false);

  // Label
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetEnabledColor(kColorAppMenuFooterButtonForeground);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetTextStyle(views::style::STYLE_BODY_5);
  label_->SetElideBehavior(gfx::ELIDE_TAIL);
  label_->GetViewAccessibility().SetIsIgnored(true);
  label_->SetProperty(views::kSkipAccessibilityPaintChecks, true);

  // Submenu arrow: shown when the action item has child actions.
  submenu_arrow_view_ = AddChildView(std::make_unique<views::ImageView>());
  submenu_arrow_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  submenu_arrow_view_->GetViewAccessibility().SetIsIgnored(true);
  submenu_arrow_view_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  submenu_arrow_view_->SetVisible(false);
}

ActionAppMenuFooterButton::~ActionAppMenuFooterButton() = default;

void ActionAppMenuFooterButton::OnPaintBackground(gfx::Canvas* canvas) {
  // Paint a rounded background highlight when hovered, pressed, or
  // keyboard-focused.
  if (GetState() == ButtonState::STATE_HOVERED ||
      GetState() == ButtonState::STATE_PRESSED || HasFocus()) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(GetColorProvider()->GetColor(
        kColorAppMenuFooterButtonBackgroundHovered));
    const float corner_radius = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_CORNER_RADIUS);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), corner_radius, flags);
  }
}

void ActionAppMenuFooterButton::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  SchedulePaint();
}

void ActionAppMenuFooterButton::OnFocus() {
  views::Button::OnFocus();
  SchedulePaint();
}

void ActionAppMenuFooterButton::OnBlur() {
  views::Button::OnBlur();
  SchedulePaint();
}

void ActionAppMenuFooterButton::SetText(std::u16string_view text) {
  label_->SetText(std::u16string(text));
  if (!text.empty()) {
    GetViewAccessibility().SetName(std::u16string(text));
    SetTooltipText(std::u16string(text));
  }
}

void ActionAppMenuFooterButton::SetImageModel(
    const ui::ImageModel& image_model) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  if (image_model.IsEmpty()) {
    icon_view_->SetVisible(false);
  } else if (image_model.IsVectorIcon()) {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *image_model.GetVectorIcon().vector_icon(),
        kColorAppMenuFooterButtonForeground, icon_size));
    icon_view_->SetVisible(true);
  } else {
    icon_view_->SetImage(image_model);
    icon_view_->SetVisible(true);
  }
}

void ActionAppMenuFooterButton::SetHasSubmenu(bool has_submenu) {
  if (!has_submenu) {
    submenu_arrow_view_->SetVisible(false);
    return;
  }
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  // Use the modern rounded arrow when rounded icons are enabled;
  // otherwise, fall back to the legacy icon.
  submenu_arrow_view_->SetImage(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kKeyboardArrowRightFlippableIcon
          : vector_icons::kSubmenuArrowChromeRefreshOldIcon,
      kColorAppMenuFooterButtonForeground, icon_size));
  submenu_arrow_view_->SetVisible(true);
}

// ActionViewInterface implementation to sync ActionItem properties to the
// ActionAppMenuFooterButton.
class ActionAppMenuFooterButtonViewInterface
    : public views::ButtonActionViewInterface {
 public:
  explicit ActionAppMenuFooterButtonViewInterface(
      ActionAppMenuFooterButton* action_view)
      : views::ButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    views::ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetText(action_item->GetText());
    if (!action_item->GetImage().IsEmpty()) {
      action_view_->SetImageModel(action_item->GetImage());
    }
    action_view_->SetHasSubmenu(!action_item->GetChildren().children().empty());
  }

 private:
  raw_ptr<ActionAppMenuFooterButton> action_view_;
};

std::unique_ptr<views::ActionViewInterface>
ActionAppMenuFooterButton::GetActionViewInterface() {
  return std::make_unique<ActionAppMenuFooterButtonViewInterface>(this);
}

BEGIN_METADATA(ActionAppMenuFooterButton)
END_METADATA
