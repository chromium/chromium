// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_signin_button_view.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

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

  const views::BoxLayout::Orientation orientation =
      views::BoxLayout::Orientation::kHorizontal;

  std::unique_ptr<views::BoxLayout> button_layout =
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), 16);

  std::unique_ptr<HoverButton> hover_button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(),
      std::make_unique<BadgedProfilePhoto>(
          BadgedProfilePhoto::BADGE_TYPE_SYNC_OFF, account_icon),
      card_title, base::ASCIIToUTF16(account_->email));

  if (orientation == views::BoxLayout::Orientation::kHorizontal) {
    button_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    hover_button->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kStart));
  }

  views::Builder<BubbleSyncPromoSigninButtonView>(this)
      .SetLayoutManager(std::move(button_layout))
      .AddChildren(views::Builder<HoverButton>(std::move(hover_button))
                       .SetBorder(std::unique_ptr<views::Border>(nullptr))
                       .SetEnabled(false),
                   views::Builder<views::MdTextButton>()
                       .SetCallback(std::move(callback))
                       .SetText(l10n_util::GetStringUTF16(
                           IDS_PROFILES_DICE_SIGNIN_BUTTON))
                       .SetProminent(true))
      .BuildChildren();
}

BubbleSyncPromoSigninButtonView::~BubbleSyncPromoSigninButtonView() = default;

BEGIN_METADATA(BubbleSyncPromoSigninButtonView, views::View)
END_METADATA
