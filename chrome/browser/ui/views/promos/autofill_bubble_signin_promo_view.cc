// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "build/buildflag.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

class AutofillBubbleSignInPromoView::DiceSigninPromoDelegate
    : public BubbleSignInPromoDelegate {
 public:
  explicit DiceSigninPromoDelegate(
      autofill::AutofillBubbleSignInPromoController* controller);
  DiceSigninPromoDelegate(const DiceSigninPromoDelegate&) = delete;
  DiceSigninPromoDelegate& operator=(const DiceSigninPromoDelegate&) = delete;
  ~DiceSigninPromoDelegate() override;

  // BubbleSignInPromoDelegate:
  void OnSignIn(const AccountInfo& account) override;

 private:
  raw_ptr<autofill::AutofillBubbleSignInPromoController> controller_;
};

AutofillBubbleSignInPromoView::DiceSigninPromoDelegate::DiceSigninPromoDelegate(
    autofill::AutofillBubbleSignInPromoController* controller)
    : controller_(controller) {
  CHECK(controller_);
}

AutofillBubbleSignInPromoView::DiceSigninPromoDelegate::
    ~DiceSigninPromoDelegate() = default;

void AutofillBubbleSignInPromoView::DiceSigninPromoDelegate::OnSignIn(
    const AccountInfo& account) {
  controller_->OnSignInToChromeClicked(account);
}

AutofillBubbleSignInPromoView::AutofillBubbleSignInPromoView(
    content::WebContents* web_contents,
    signin::SignInAutofillBubblePromoType promo_type)
    // TODO(crbug.com/319411728): Make this dependant on type (for now only
    // password).
    : controller_(PasswordsModelDelegateFromWebContents(web_contents)),
      promo_type_(promo_type) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));
  dice_sign_in_promo_delegate_ =
      std::make_unique<AutofillBubbleSignInPromoView::DiceSigninPromoDelegate>(
          &controller_);

  int message_resource_id = 0;
  switch (promo_type_) {
    // TODO(crbug.com/319411728): Add the correct strings per type.
    case signin::SignInAutofillBubblePromoType::Payments:
    case signin::SignInAutofillBubblePromoType::Addresses:
    case signin::SignInAutofillBubblePromoType::Passwords:
      message_resource_id = IDS_PASSWORD_MANAGER_DICE_PROMO_SIGNIN_MESSAGE;
  }
  AddChildView(new BubbleSignInPromoView(
      profile, dice_sign_in_promo_delegate_.get(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
      message_resource_id, ui::ButtonStyle::kDefault,
      views::style::STYLE_PRIMARY));
}

AutofillBubbleSignInPromoView::~AutofillBubbleSignInPromoView() = default;

BEGIN_METADATA(AutofillBubbleSignInPromoView)
END_METADATA
