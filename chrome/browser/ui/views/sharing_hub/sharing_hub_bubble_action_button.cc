// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace sharing_hub {

namespace {

// These values values come directly from the Figma redlines. See
// https://crbug.com/1314486 and https://crbug.com/1343564.
static constexpr gfx::Insets kInteriorMargin = gfx::Insets::VH(10, 16);
static constexpr gfx::Insets kDefaultMargin = gfx::Insets::VH(0, 16);
static constexpr gfx::Size kPrimaryIconSize{16, 16};

// The layout will break if this icon isn't square - you may need to adjust the
// vector icon creation below.
static_assert(kPrimaryIconSize.width() == kPrimaryIconSize.height());

ui::ImageModel ImageForAction(const SharingHubAction& action_info) {
  if (!action_info.third_party_icon.isNull())
    return ui::ImageModel::FromImageSkia(action_info.third_party_icon);
  return ui::ImageModel::FromVectorIcon(*action_info.icon, ui::kColorMenuIcon,
                                        kPrimaryIconSize.width());
}

}  // namespace

SharingHubBubbleActionButton::SharingHubBubbleActionButton(
    SharingHubBubbleViewImpl* bubble,
    const SharingHubAction& action_info)
    : action_command_id_(action_info.command_id),
      action_is_first_party_(action_info.is_first_party),
      action_name_for_metrics_(action_info.feature_name_for_metrics) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMargin)
      .SetDefault(views::kMarginsKey, kDefaultMargin)
      .SetCollapseMargins(true);

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetEnabled(true);
  SetBackground(views::CreateThemedSolidBackground(ui::kColorMenuBackground));
  SetCallback(base::BindRepeating(&SharingHubBubbleViewImpl::OnActionSelected,
                                  base::Unretained(bubble),
                                  base::Unretained(this)));

  image_ = AddChildView(
      std::make_unique<views::ImageView>(ImageForAction(action_info)));
  image_->SetImageSize(kPrimaryIconSize);
  image_->SetCanProcessEventsWithinSubtree(false);

  title_ = AddChildView(std::make_unique<views::Label>(
      action_info.title, views::style::CONTEXT_MENU));
  title_->SetCanProcessEventsWithinSubtree(false);

  if (action_is_first_party_) {
    GetViewAccessibility().OverrideName(title_->GetText());
  } else {
    GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
        IDS_SHARING_HUB_SHARE_LABEL_ACCESSIBILITY, title_->GetText()));
  }
}

SharingHubBubbleActionButton::~SharingHubBubbleActionButton() = default;

void SharingHubBubbleActionButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateColors();
}

void SharingHubBubbleActionButton::OnFocus() {
  views::Button::OnFocus();
  UpdateColors();
}

void SharingHubBubbleActionButton::OnBlur() {
  views::Button::OnBlur();
  UpdateColors();
}

void SharingHubBubbleActionButton::StateChanged(
    views::Button::ButtonState old_state) {
  views::Button::StateChanged(old_state);
  UpdateColors();
}

void SharingHubBubbleActionButton::UpdateColors() {
  bool draw_focus = GetState() == STATE_HOVERED || HasFocus();
  // Pretend to be a menu item:
  SkColor bg_color = GetColorProvider()->GetColor(
      // Note: selected vs highlighted seems strange here; highlighted is more
      // in line with what's happening. The two colors are almost the same but
      // selected gives better behavior in high contrast.
      draw_focus ? ui::kColorMenuItemBackgroundSelected
                 : ui::kColorMenuBackground);

  SetBackground(views::CreateSolidBackground(bg_color));
  title_->SetBackgroundColor(bg_color);
  title_->SetTextStyle(draw_focus ? views::style::STYLE_SELECTED
                                  : views::style::STYLE_PRIMARY);
}

BEGIN_METADATA(SharingHubBubbleActionButton, Button)
END_METADATA

}  // namespace sharing_hub
