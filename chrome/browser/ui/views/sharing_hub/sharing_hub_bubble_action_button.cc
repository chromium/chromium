// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"

namespace sharing_hub {

namespace {

static constexpr int kPrimaryIconSize = 20;
constexpr auto kPrimaryIconBorder = gfx::Insets(6);

std::unique_ptr<views::ImageView> CreateIconFromVector(
    const gfx::VectorIcon& vector_icon) {
  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::kColorMenuIcon, kPrimaryIconSize));
  icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon;
}

std::unique_ptr<views::ImageView> CreateIconFromImageSkia(
    const gfx::ImageSkia& png_icon) {
  // The icon size has to be defined later if the image will be visible.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImageSize(gfx::Size(kPrimaryIconSize, kPrimaryIconSize));
  icon->SetImage(png_icon);
  icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon;
}

}  // namespace

SharingHubBubbleActionButton::SharingHubBubbleActionButton(
    SharingHubBubbleViewImpl* bubble,
    const SharingHubAction& action_info)
    : HoverButton(
          base::BindRepeating(&SharingHubBubbleViewImpl::OnActionSelected,
                              base::Unretained(bubble),
                              base::Unretained(this)),

          action_info.third_party_icon.isNull()
              ? CreateIconFromVector(*action_info.icon)
              : CreateIconFromImageSkia(action_info.third_party_icon),
          action_info.title),
      action_command_id_(action_info.command_id),
      action_is_first_party_(action_info.is_first_party),
      action_name_for_metrics_(action_info.feature_name_for_metrics) {
  SetEnabled(true);

  title()->SetTextContext(views::style::CONTEXT_MENU);
  SetBackground(
      views::CreateThemedSolidBackground(this, ui::kColorMenuBackground));
}

SharingHubBubbleActionButton::~SharingHubBubbleActionButton() = default;

void SharingHubBubbleActionButton::UpdateBackgroundColor() {
  // Pretend to be a menu item:
  SkColor bg_color =
      GetColorProvider()->GetColor(GetVisualState() == STATE_HOVERED
                                       ? ui::kColorMenuItemBackgroundHighlighted
                                       : ui::kColorMenuBackground);

  SetBackground(views::CreateSolidBackground(bg_color));
  SetTitleTextStyle(
      // Give the hovered element the "highlighted" menu styling - otherwise the
      // text color won't change appropriately to keep up with the background
      // color changing in high contrast mode.
      GetVisualState() == STATE_HOVERED ? views::style::STYLE_HIGHLIGHTED
                                        : views::style::STYLE_PRIMARY,
      bg_color);
}

void SharingHubBubbleActionButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  HoverButton::GetAccessibleNodeData(node_data);

  // TODO(ellyjones): Yikes!
  // This hack works around https://crbug.com/1245744, which is a design flaw in
  // HoverButton. In particular, HoverButton overwrites the accessible name its
  // client configured on it whenever it bounds change (!) with the plain
  // concatenation of its title and subtitle, which makes it impossible to set
  // an accessible name with extra context, as needed for
  // https://crbug.com/1230178.
  // TODO(https://crbug.com/1245744): Remove this.
  if (!action_is_first_party_) {
    node_data->SetName(l10n_util::GetStringFUTF16(
        IDS_SHARING_HUB_SHARE_LABEL_ACCESSIBILITY, title()->GetText()));
  }
}

void SharingHubBubbleActionButton::OnThemeChanged() {
  HoverButton::OnThemeChanged();
  UpdateBackgroundColor();
}

BEGIN_METADATA(SharingHubBubbleActionButton, HoverButton)
END_METADATA

}  // namespace sharing_hub
