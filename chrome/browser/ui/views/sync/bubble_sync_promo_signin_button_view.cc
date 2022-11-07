// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_signin_button_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"

BubbleSyncPromoSigninButtonView::BubbleSyncPromoSigninButtonView(
    views::Button::PressedCallback callback,
    bool prominent)
    : account_(absl::nullopt) {
  views::Builder<BubbleSyncPromoSigninButtonView>(this)
      .SetUseDefaultFillLayout(true)
      .AddChild(
          // Regular MD text button when there is no account.
          views::Builder<views::MdTextButton>()
              .SetCallback(std::move(callback))
              .SetText(
                  l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON))
              .SetProminent(prominent))
      .BuildChildren();
}

BubbleSyncPromoSigninButtonView::BubbleSyncPromoSigninButtonView(
    const AccountInfo& account,
    const gfx::Image& account_icon,
    views::Button::PressedCallback callback,
    bool use_account_name_as_title)
    : account_(account) {
  DCHECK(!account_icon.IsEmpty());
  auto card_title =
      use_account_name_as_title
          ? base::UTF8ToUTF16(account.full_name)
          : l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE);

  views::Builder<BubbleSyncPromoSigninButtonView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 16))
      .AddChildren(
          views::Builder<HoverButton>(
              std::make_unique<HoverButton>(
                  views::Button::PressedCallback(),
                  std::make_unique<BadgedProfilePhoto>(
                      BadgedProfilePhoto::BADGE_TYPE_SYNC_OFF, account_icon),
                  card_title, base::ASCIIToUTF16(account_->email)))
              .SetBorder(std::unique_ptr<views::Border>(nullptr))
              .SetEnabled(false),
          views::Builder<views::MdTextButton>()
              .SetCallback(std::move(callback))
              .SetText(
                  l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON))
              .SetProminent(true))
      .BuildChildren();
}

BubbleSyncPromoSigninButtonView::~BubbleSyncPromoSigninButtonView() = default;

BEGIN_METADATA(BubbleSyncPromoSigninButtonView, views::View)
END_METADATA
