// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace sharing_hub {

namespace {

static constexpr int kPrimaryIconSize = 20;
constexpr auto kPrimaryIconBorder = gfx::Insets(6);

std::unique_ptr<views::ImageView> CreateIconFromVector(
    const gfx::VectorIcon& vector_icon) {
  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::NativeTheme::kColorId_DefaultIconColor,
      kPrimaryIconSize));
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
}

SharingHubBubbleActionButton::~SharingHubBubbleActionButton() = default;

BEGIN_METADATA(SharingHubBubbleActionButton, HoverButton)
END_METADATA

}  // namespace sharing_hub
