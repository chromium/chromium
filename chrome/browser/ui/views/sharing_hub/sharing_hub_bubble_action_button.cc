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
  return ui::ImageModel::FromVectorIcon(*action_info.icon, ui::kColorMenuIcon,
                                        kPrimaryIconSize.width());
}

}  // namespace

SharingHubBubbleActionButton::SharingHubBubbleActionButton(
    SharingHubBubbleViewImpl* bubble,
    const SharingHubAction& action_info)
    : action_info_(action_info) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMargin)
      .SetDefault(views::kMarginsKey, kDefaultMargin)
      .SetCollapseMargins(true);

  // Even though this is a button, which would normally use ACCESSIBLE_ONLY,
  // we really do want ALWAYS here. Visually, these buttons are presented as
  // a menu, which means we need these to be keyboard-traversable with the
  // arrow keys to match the behavior of other menus, and they're only keyboard
  // traversable when focusable.
  //
  // This creates a different divergence from menu behavior: the individual
  // items in the sharing hub are also tab-traversable, while items in a menu
  // are not. That's annoying, but not as bad as the surface being keyboard
  // inaccessible, so we live with it.
  //
  // See https://crbug.com/1404226 and https://crbug.com/1323053.
  SetFocusBehavior(FocusBehavior::ALWAYS);

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

  GetViewAccessibility().SetName(title_->GetText(),
                                 ax::mojom::NameFrom::kAttribute);
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

BEGIN_METADATA(SharingHubBubbleActionButton)
END_METADATA

}  // namespace sharing_hub
