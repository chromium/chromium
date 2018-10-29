// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

DiceBubbleSyncPromoView::DiceBubbleSyncPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    int no_accounts_promo_message_resource_id,
    int accounts_promo_message_resource_id,
    bool signin_button_prominent,
    int text_style)
    : views::View(), delegate_(delegate) {
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));

  std::vector<AccountInfo> accounts =
      signin_ui_util::GetAccountsForDicePromos(profile);
  // Always show the accounts promo message for now.
  const int title_resource_id = accounts_promo_message_resource_id;

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()
          ->GetDialogInsetsForContentType(views::TEXT, views::TEXT)
          .bottom());
  SetLayoutManager(std::move(layout));

  if (title_resource_id) {
    base::string16 title_text = l10n_util::GetStringUTF16(title_resource_id);
    views::Label* title =
        new views::Label(title_text, CONTEXT_BODY_TEXT_LARGE, text_style);
    title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title->SetMultiLine(true);
    AddChildView(title);
  }

  if (accounts.empty()) {
    signin_button_view_ =
        new DiceSigninButtonView(this, signin_button_prominent);
  } else {
    gfx::Image account_icon =
        AccountTrackerServiceFactory::GetForProfile(profile)->GetAccountImage(
            accounts[0].account_id);
    if (account_icon.IsEmpty()) {
      account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
    }
    signin_button_view_ = new DiceSigninButtonView(
        accounts[0], account_icon, this, /*show_drop_down_arrow=*/false,
        /*use_account_name_as_title=*/true);

    // Store account information for submenu.
    accounts_for_submenu_.assign(accounts.begin() + 1, accounts.end());
    AccountTrackerService* tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile);
    for (auto account : accounts_for_submenu_) {
      images_for_submenu_.push_back(
          tracker_service->GetAccountImage(account.account_id));
    }
  }
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      access_point, !accounts.empty() /* with_account */);
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

  if (sender == signin_button_view_->drop_down_arrow()) {
    // Display a submenu listing the GAIA web accounts (except the first one).
    // Using base::Unretained(this) is safe here because |dice_accounts_menu_|
    // is owned by |DiceBubbleSyncPromoView|, i.e. |this|.
    dice_accounts_menu_ = std::make_unique<DiceAccountsMenu>(
        accounts_for_submenu_, images_for_submenu_,
        base::BindOnce(&DiceBubbleSyncPromoView::EnableSync,
                       base::Unretained(this),
                       false /* is_default_promo_account */));
    dice_accounts_menu_->Show(signin_button_view_,
                              signin_button_view_->drop_down_arrow());
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
