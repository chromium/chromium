// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/sign_in_celebration_handler.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/l10n/l10n_util.h"

SignInCelebrationHandler::SignInCelebrationHandler(
    signin::IdentityManager* identity_manager,
    mojo::PendingRemote<intro::mojom::Page> page,
    mojo::PendingReceiver<intro::mojom::PageHandler> receiver)
    : identity_manager_(CHECK_DEREF(identity_manager)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  identity_manager_observation_.Observe(&identity_manager_.get());
}

SignInCelebrationHandler::~SignInCelebrationHandler() = default;

void SignInCelebrationHandler::GetSignInCelebrationUserInfo(
    GetSignInCelebrationUserInfoCallback callback) {
  std::move(callback).Run(GetUserInformationMojo());
}

void SignInCelebrationHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id == identity_manager_->GetPrimaryAccountId(
                             signin::ConsentLevel::kSignin) &&
      (info.GetGivenName().has_value() || info.GetAvatarImage().has_value())) {
    UpdateUserInfo();
  }
}

void SignInCelebrationHandler::UpdateUserInfo() {
  if (page_.is_bound()) {
    page_->OnSignInCelebrationUserInfoUpdated(GetUserInformationMojo());
  }
}

intro::mojom::SignInCelebrationUserInfoPtr
SignInCelebrationHandler::GetUserInformationMojo() {
  auto user_info = intro::mojom::SignInCelebrationUserInfo::New();

  const CoreAccountInfo core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfo(core_account_info);
  const std::string display_name = std::string(
      account_info.GetGivenName().value_or(core_account_info.email));

  user_info->title =
      l10n_util::GetStringFUTF8(IDS_FRE_SIGN_IN_CELEBRATION_WELCOME_TITLE,
                                base::UTF8ToUTF16(display_name));

  user_info->avatar_url = GURL(signin::GetAccountPictureUrl(account_info));

  return user_info;
}
