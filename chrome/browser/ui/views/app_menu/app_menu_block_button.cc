// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_block_button.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

AppMenuBlockButton::AppMenuBlockButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  const auto* provider = ChromeLayoutProvider::Get();
  const int icon_size = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_ICON_SIZE);
  const int between_spacing = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_BETWEEN_CHILD_SPACING);
  const int corner_radius =
      provider->GetCornerRadiusMetric(kActionAppMenuBlockEntryCornerRadius);

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_ENTRY_BUTTON),
      between_spacing);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);

  SetBackground(views::CreateRoundedRectBackground(
      kColorAppMenuBlockButtonBackground, corner_radius));

  // Enable keyboard navigation and focus highlighting.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);

  auto* const ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kAbove);
  ink_drop->SetBaseColor(kColorAppMenuBlockButtonBackgroundHovered);
  ink_drop->SetVisibleOpacity(1.0f);
  ink_drop->SetHighlightOpacity(1.0f);
  SetShowInkDropWhenHotTracked(true);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                corner_radius);

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->GetViewAccessibility().SetIsIgnored(true);
  icon_view_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  icon_view_->SetCanProcessEventsWithinSubtree(false);

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetTextStyle(views::style::STYLE_BODY_5);
  label_->SetLineHeight(provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_LINE_HEIGHT));
  label_->SetMultiLine(true);
  label_->SetMaxLines(2);
  label_->SetElideBehavior(gfx::ELIDE_TAIL);
  label_->GetViewAccessibility().SetIsIgnored(true);
  label_->SetProperty(views::kSkipAccessibilityPaintChecks, true);
  label_->SetCanProcessEventsWithinSubtree(false);

  corner_radius_ = corner_radius;
  UpdateColors();
}

AppMenuBlockButton::~AppMenuBlockButton() = default;

gfx::Size AppMenuBlockButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const auto* provider = ChromeLayoutProvider::Get();
  const int width =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_WIDTH);
  const int min_height =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_HEIGHT);
  const int preferred_height =
      views::Button::CalculatePreferredSize(views::SizeBounds(width, {}))
          .height();
  return gfx::Size(width, std::max(min_height, preferred_height));
}

void AppMenuBlockButton::OnEnabledChanged() {
  views::Button::OnEnabledChanged();

  auto* const ink_drop_host = views::InkDrop::Get(this);
  if (GetState() == STATE_DISABLED) {
    ink_drop_host->SetMode(views::InkDropHost::InkDropMode::OFF);
  } else {
    ink_drop_host->SetMode(views::InkDropHost::InkDropMode::ON);
    ink_drop_host->GetInkDrop()->SetHovered(IsMouseHovered());
  }

  UpdateColors();
}

void AppMenuBlockButton::SetText(std::u16string_view text) {
  label_->SetText(std::u16string(text));
  if (!text.empty()) {
    GetViewAccessibility().SetName(std::u16string(text));
    SetTooltipText(std::u16string(text));
  }
}

void AppMenuBlockButton::SetImageModel(const ui::ImageModel& image_model) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_BLOCK_ENTRY_ICON_SIZE);
  if (image_model.IsVectorIcon()) {
    const ui::ColorId icon_color =
        GetState() == STATE_DISABLED
            ? ui::ColorId{ui::kColorButtonForegroundDisabled}
            : ui::ColorId{kColorAppMenuBlockButtonForeground};
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *image_model.GetVectorIcon().vector_icon(), icon_color, icon_size));
  } else {
    icon_view_->SetImage(image_model);
  }
}

void AppMenuBlockButton::UpdateColors() {
  const bool is_disabled = GetState() == STATE_DISABLED;
  SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/1, corner_radius_,
      is_disabled ? ui::ColorId{ui::kColorButtonBorderDisabled}
                  : ui::ColorId{kColorAppMenuBlockButtonBorder}));
  label_->SetEnabledColor(
      is_disabled ? ui::ColorId{ui::kColorButtonForegroundDisabled}
                  : ui::ColorId{kColorAppMenuBlockButtonForeground});
  if (!icon_view_->GetImageModel().IsEmpty()) {
    SetImageModel(icon_view_->GetImageModel());
  }
}

class AppMenuBlockButtonActionViewInterface
    : public views::ButtonActionViewInterface {
 public:
  explicit AppMenuBlockButtonActionViewInterface(
      AppMenuBlockButton* action_view)
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
    const std::u16string_view accessible_name =
        action_item->GetAccessibleName();
    if (!accessible_name.empty()) {
      action_view_->GetViewAccessibility().SetName(
          std::u16string(accessible_name));
    }

    std::u16string tooltip = std::u16string(action_item->GetTooltipText());
    if (tooltip.empty()) {
      tooltip = std::u16string(action_item->GetText());
    }
    const ui::Accelerator& accel = action_item->GetAccelerator();
    if (!tooltip.empty() && accel.key_code() != ui::VKEY_UNKNOWN &&
        !accel.GetShortcutText().empty()) {
      tooltip = l10n_util::GetStringFUTF16(IDS_APP_MENU_BLOCK_TOOLTIP, tooltip,
                                           accel.GetShortcutText());
    }
    action_view_->SetTooltipText(tooltip);

    if (!action_item->GetImage().IsEmpty()) {
      action_view_->SetImageModel(action_item->GetImage());
    }
  }

 private:
  raw_ptr<AppMenuBlockButton> action_view_;
};

std::unique_ptr<views::ActionViewInterface>
AppMenuBlockButton::GetActionViewInterface() {
  return std::make_unique<AppMenuBlockButtonActionViewInterface>(this);
}

BEGIN_METADATA(AppMenuBlockButton)
END_METADATA
