// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_sign_in_promo_view.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

PasswordSignInPromoView::DiceSyncPromoDelegate::DiceSyncPromoDelegate(
    SignInPromoBubbleController* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

PasswordSignInPromoView::DiceSyncPromoDelegate::~DiceSyncPromoDelegate() =
    default;

void PasswordSignInPromoView::DiceSyncPromoDelegate::OnEnableSync(
    const AccountInfo& account) {
  controller_->OnSignInToChromeClicked(account);
}

PasswordSignInPromoView::PasswordSignInPromoView(
    content::WebContents* web_contents)
    : controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  Profile* profile = controller_.GetProfile();
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));
  dice_sync_promo_delegate_ =
      std::make_unique<PasswordSignInPromoView::DiceSyncPromoDelegate>(
          &controller_);
  AddChildView(new DiceBubbleSyncPromoView(
      profile, dice_sync_promo_delegate_.get(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
      IDS_PASSWORD_MANAGER_DICE_PROMO_SIGNIN_MESSAGE,
      IDS_PASSWORD_MANAGER_DICE_PROMO_SYNC_MESSAGE));
}

PasswordSignInPromoView::~PasswordSignInPromoView() = default;

BEGIN_METADATA(PasswordSignInPromoView, views::View)
END_METADATA
