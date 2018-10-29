// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

constexpr int kDropDownArrowIconSize = 12;
constexpr int kPromoAccountImageSize = 40;

}  // namespace

DiceSigninButtonView::DiceSigninButtonView(
    views::ButtonListener* button_listener,
    bool prominent)
    : account_(base::nullopt) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Regular MD text button when there is no account.
  views::MdTextButton* button = views::MdTextButton::Create(
      button_listener,
      l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
      views::style::CONTEXT_BUTTON);
  button->SetProminent(prominent);
  AddChildView(button);
  signin_button_ = button;
}

DiceSigninButtonView::DiceSigninButtonView(
    const AccountInfo& account,
    const gfx::Image& account_icon,
    views::ButtonListener* button_listener,
    bool show_drop_down_arrow,
    bool use_account_name_as_title)
    : account_(account) {
  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  grid_layout->StartRow(views::GridLayout::kFixedSize, 0);

  // Add a stretching column for the account card.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);

  DCHECK(!account_icon.IsEmpty());
  auto account_icon_view = std::make_unique<views::ImageView>();
  account_icon_view->SetImage(
      profiles::GetSizedAvatarIcon(account_icon, true, kPromoAccountImageSize,
                                   kPromoAccountImageSize,
                                   profiles::SHAPE_CIRCLE)
          .AsImageSkia());
  auto card_title =
      use_account_name_as_title
          ? base::UTF8ToUTF16(account.full_name)
          : l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE);
  HoverButton* account_card =
      new HoverButton(button_listener, std::move(account_icon_view), card_title,
                      base::ASCIIToUTF16(account_->email));
  account_card->SetBorder(nullptr);
  account_card->SetEnabled(false);
  grid_layout->AddView(account_card);

  if (show_drop_down_arrow) {
    // Add a non-stretching column for the the drop down arrow.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::USE_PREF, 0, 0);
    arrow_ = new HoverButton(
        button_listener,
        gfx::CreateVectorIcon(kSigninButtonDropDownArrowIcon,
                              kDropDownArrowIconSize, SK_ColorBLACK),
        base::string16());
    arrow_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_PROFILES_DICE_SIGNIN_WITH_ANOTHER_ACCOUNT_BUTTON));
    grid_layout->AddView(arrow_);
  }

  grid_layout->AddPaddingRow(views::GridLayout::kFixedSize, 16);

  columns = grid_layout->AddColumnSet(1);
  grid_layout->StartRow(views::GridLayout::kFixedSize, 1);
  // Add a stretching column for the sign in button.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::TRAILING, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);
  views::MdTextButton* button = views::MdTextButton::Create(
      button_listener,
      l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
      views::style::CONTEXT_BUTTON);
  button->SetProminent(true);
  grid_layout->AddView(button);
  signin_button_ = button;
}

DiceSigninButtonView::~DiceSigninButtonView() = default;
