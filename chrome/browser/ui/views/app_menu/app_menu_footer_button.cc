// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_footer_button.h"

#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kRowLineHeight = 16;
}  // namespace

AppMenuFooterButton::AppMenuFooterButton(views::MenuItemView* submenu_item) {
  const auto* provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  const int between_spacing = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_BETWEEN_CHILD_SPACING);

  // Arrange the button's icon, label, and optional submenu arrow in a row
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      between_spacing));
  layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);

  // Enable keyboard navigation and focus highlighting.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  auto* const ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kAbove);
  ink_drop->SetBaseColor(kColorAppMenuFooterButtonBackgroundHovered);
  ink_drop->SetVisibleOpacity(1.0f);
  ink_drop->SetHighlightOpacity(1.0f);
  SetShowInkDropWhenHotTracked(true);

  // Icon: hidden by default until populated with an ImageModel.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->GetViewAccessibility().SetIsIgnored(true);
  icon_view_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  icon_view_->SetCanProcessEventsWithinSubtree(false);
  icon_view_->SetVisible(false);

  // Label
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetElideBehavior(gfx::ELIDE_TAIL);
  label_->GetViewAccessibility().SetIsIgnored(true);
  label_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  label_->SetCanProcessEventsWithinSubtree(false);

  if (submenu_item) {
    submenu_item->SetAnchorView(this);

    submenu_arrow_view_ = AddChildView(std::make_unique<views::ImageView>());
    submenu_arrow_view_->SetImageSize(gfx::Size(icon_size, icon_size));
    submenu_arrow_view_->GetViewAccessibility().SetIsIgnored(true);
    submenu_arrow_view_->SetProperty(views::kSkipAccessibilityPaintChecks,
                                     true);
    submenu_arrow_view_->SetCanProcessEventsWithinSubtree(false);
    submenu_arrow_view_->SetImage(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled()
            ? vector_icons::kKeyboardArrowRightFlippableIcon
            : vector_icons::kSubmenuArrowChromeRefreshOldIcon,
        kColorAppMenuFooterButtonForeground, icon_size));
  }
}

AppMenuFooterButton::~AppMenuFooterButton() {
  if (auto* submenu_item = GetSubmenuItem()) {
    submenu_item->SetAnchorView(nullptr);
  }
}

views::MenuItemView* AppMenuFooterButton::GetSubmenuItem() const {
  return GetProperty(views::kSubmenuItemKey);
}

void AppMenuFooterButton::SetText(std::u16string_view text) {
  label_->SetText(std::u16string(text));
  if (!text.empty()) {
    GetViewAccessibility().SetName(std::u16string(text));
  }
}

void AppMenuFooterButton::SetImageModel(const ui::ImageModel& image_model) {
  if (GetSubmenuItem() || image_model.IsEmpty()) {
    icon_view_->SetVisible(false);
  } else if (image_model.IsVectorIcon()) {
    const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_ICON_SIZE);
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *image_model.GetVectorIcon().vector_icon(),
        use_row_style_ ? ui::ColorId{ui::kColorMenuIcon}
                       : ui::ColorId{kColorAppMenuFooterButtonForeground},
        icon_size));
    icon_view_->SetVisible(true);
  } else {
    icon_view_->SetImage(image_model);
    icon_view_->SetVisible(true);
  }
}

void AppMenuFooterButton::SetUseRowStyle(bool use_row_style) {
  use_row_style_ = use_row_style;
  const auto* provider = ChromeLayoutProvider::Get();
  if (use_row_style_) {
    label_->SetTextContext(views::style::CONTEXT_MENU);
    label_->SetTextStyle(views::style::STYLE_BODY_4);
    label_->SetLineHeight(kRowLineHeight);
    label_->SetEnabledColor(ui::kColorMenuItemForeground);

    views::InstallRectHighlightPathGenerator(this);
    SetBackground(views::CreateSolidBackground(ui::kColorMenuBackground));

    const int vertical_padding = provider->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_FOOTER_BOTTOM_CONTAINER_SPACING);
    const int horizontal_padding =
        provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN).left() +
        provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_BUTTON).left();
    layout_->set_inside_border_insets(
        gfx::Insets::VH(vertical_padding, horizontal_padding));
  } else {
    label_->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
    label_->SetEnabledColor(kColorAppMenuFooterButtonForeground);

    const int corner_radius = provider->GetDistanceMetric(
        DISTANCE_ACTION_APP_MENU_FOOTER_BUTTON_CORNER_RADIUS);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  corner_radius);
    SetBackground(views::CreateRoundedRectBackground(ui::kColorMenuBackground,
                                                     corner_radius));

    layout_->set_inside_border_insets(
        provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_BUTTON));
  }
  if (!icon_view_->GetImageModel().IsEmpty()) {
    SetImageModel(icon_view_->GetImageModel());
  }
}

// ActionViewInterface implementation to sync ActionItem properties to the
// AppMenuFooterButton.
class AppMenuFooterButtonViewInterface
    : public views::ButtonActionViewInterface {
 public:
  explicit AppMenuFooterButtonViewInterface(AppMenuFooterButton* action_view)
      : views::ButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    views::ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetText(action_item->GetText());
    if (!action_item->GetImage().IsEmpty()) {
      action_view_->SetImageModel(action_item->GetImage());
    }
    action_view_->SetTooltipText(std::u16string());
  }

 private:
  raw_ptr<AppMenuFooterButton> action_view_;
};

std::unique_ptr<views::ActionViewInterface>
AppMenuFooterButton::GetActionViewInterface() {
  return std::make_unique<AppMenuFooterButtonViewInterface>(this);
}

BEGIN_METADATA(AppMenuFooterButton)
END_METADATA
