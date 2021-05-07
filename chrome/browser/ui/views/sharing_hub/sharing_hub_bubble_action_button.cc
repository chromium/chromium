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
#include "ui/views/controls/color_tracking_icon_view.h"

namespace sharing_hub {

namespace {

std::unique_ptr<views::ColorTrackingIconView> CreateIcon(
    const gfx::VectorIcon& vector_icon) {
  static constexpr int kPrimaryIconSize = 20;
  auto icon = std::make_unique<views::ColorTrackingIconView>(vector_icon,
                                                             kPrimaryIconSize);
  constexpr auto kPrimaryIconBorder = gfx::Insets(6);
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
          CreateIcon(action_info.icon),
          l10n_util::GetStringUTF16(action_info.title)) {
  action_command_id_ = action_info.command_id;
  action_is_first_party_ = action_info.is_first_party;
  SetEnabled(true);
}

SharingHubBubbleActionButton::~SharingHubBubbleActionButton() = default;

BEGIN_METADATA(SharingHubBubbleActionButton, HoverButton)
END_METADATA

}  // namespace sharing_hub
