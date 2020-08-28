// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

DiceBubbleSyncPromoView::DiceBubbleSyncPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    int accounts_promo_message_resource_id,
    bool signin_button_prominent,
    int text_style)
    : delegate_(delegate) {
  DCHECK(!profile->IsGuestSession());
  AccountInfo account;
  // Signin promos can be shown in incognito, they use an empty account list.
  if (profile->IsRegularProfile())
    account = signin_ui_util::GetSingleAccountForDicePromos(profile);

  // Always show the accounts promo message for now.
  const int title_resource_id = accounts_promo_message_resource_id;

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()
          ->GetDialogInsetsForContentType(views::TEXT, views::TEXT)
          .bottom());
  SetLayoutManager(std::move(layout));

  if (title_resource_id) {
    base::string16 title_text = l10n_util::GetStringUTF16(title_resource_id);
    views::Label* title = new views::Label(
        title_text, views::style::CONTEXT_DIALOG_BODY_TEXT, text_style);
    title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title->SetMultiLine(true);
    AddChildView(title);
  }

  if (account.IsEmpty()) {
    signin_button_view_ =
        new DiceSigninButtonView(this, signin_button_prominent);
  } else {
    gfx::Image account_icon = account.account_image;
    if (account_icon.IsEmpty()) {
      account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
    }
    signin_button_view_ =
        new DiceSigninButtonView(account, account_icon, this,
                                 /*use_account_name_as_title=*/true);
  }
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      access_point, !account.IsEmpty() /* with_account */);
  AddChildView(signin_button_view_);
}

DiceBubbleSyncPromoView::~DiceBubbleSyncPromoView() = default;

void DiceBubbleSyncPromoView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (sender == signin_button_view_->signin_button()) {
    EnableSync(true /* is_default_promo_account */,
               signin_button_view_->account());
    return;
  }
  NOTREACHED();
}

views::View* DiceBubbleSyncPromoView::GetSigninButtonForTesting() {
  return signin_button_view_ ? signin_button_view_->signin_button() : nullptr;
}

void DiceBubbleSyncPromoView::EnableSync(
    bool is_default_promo_account,
    const base::Optional<AccountInfo>& account) {
  delegate_->OnEnableSync(account.value_or(AccountInfo()),
                          is_default_promo_account);
}

const char* DiceBubbleSyncPromoView::GetClassName() const {
  return "DiceBubbleSyncPromoView";
}
