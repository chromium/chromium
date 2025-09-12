// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_util.h"

#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace save_to_drive {

std::unique_ptr<views::View> CreateAccountRow(const AccountInfo& account) {
  return views::Builder<views::BoxLayoutView>()
      .SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST))
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .AddChildren(
          // Avatar image.
          views::Builder<views::ImageView>().SetImage(
              ui::ImageModel::FromImage(GetSizedAvatarIcon(
                  account.account_image, kAvatarSize, kAvatarSize,
                  profiles::AvatarShape::SHAPE_CIRCLE))),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .AddChildren(
                  // Full name.
                  views::Builder<views::Label>()
                      .SetText(base::UTF8ToUTF16(account.full_name))
                      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                      .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
                      .SetEnabledColor(ui::kColorSysOnSurface)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT),
                  // Email.
                  views::Builder<views::Label>()
                      .SetText(base::UTF8ToUTF16(account.email))
                      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                      .SetTextStyle(views::style::STYLE_BODY_4)
                      .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT)))
      .Build();
}

}  // namespace save_to_drive
