// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/work_profile_confirmation_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/work_profile_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using signin::ConsentLevel;

namespace {
const int kProfileImageSize = 128;
}  // namespace

WorkProfileConfirmationHandler::WorkProfileConfirmationHandler(
    Profile* profile,
    Browser* browser,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback)
    : profile_(profile),
      browser_(browser),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      callback_(std::move(callback)) {
  DCHECK(profile_);
  BrowserList::AddObserver(this);
}

WorkProfileConfirmationHandler::~WorkProfileConfirmationHandler() {
  BrowserList::RemoveObserver(this);
  identity_manager_->RemoveObserver(this);

  // Abort signin and prevent work profile creation if none of the actions on
  // the work profile confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact_) {
    CloseModalSigninWindow(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
    base::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
  }
}

void WorkProfileConfirmationHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void WorkProfileConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm",
      base::BindRepeating(&WorkProfileConfirmationHandler::HandleConfirm,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::BindRepeating(&WorkProfileConfirmationHandler::HandleCancel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(
          &WorkProfileConfirmationHandler::HandleInitializedWithSize,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "accountImageRequest",
      base::BindRepeating(
          &WorkProfileConfirmationHandler::HandleAccountImageRequest,
          base::Unretained(this)));
}

void WorkProfileConfirmationHandler::HandleConfirm(
    const base::ListValue* args) {
  did_user_explicitly_interact_ = true;
  CloseModalSigninWindow(DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE);
}

void WorkProfileConfirmationHandler::HandleCancel(const base::ListValue* args) {
  did_user_explicitly_interact_ = true;
  CloseModalSigninWindow(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
}

void WorkProfileConfirmationHandler::HandleAccountImageRequest(
    const base::ListValue* args) {
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kNotRequired));

  // Fire the "account-image-changed" listener from |SetUserImageURL()|.
  // Note: If the account info is not available yet in the
  // IdentityManager, i.e. account_info is empty, the listener will be
  // fired again through |OnAccountUpdated()|.
  if (primary_account_info)
    SetUserImageURL(primary_account_info->picture_url);
}

void WorkProfileConfirmationHandler::SetUserImageURL(
    const std::string& picture_url) {
  GURL picture_gurl(picture_url);
  if (!picture_gurl.is_valid()) {
    // As long as the provided gaia picture is not valid, stick to the default
    // avatar provided in the load-time data.
    return;
  }
  GURL picture_gurl_with_options = signin::GetAvatarImageURLWithOptions(
      picture_gurl, kProfileImageSize, false /* no_silhouette */);
  base::Value picture_url_value(picture_gurl_with_options.spec());

  AllowJavascript();
  FireWebUIListener("account-image-changed", picture_url_value);
}

void WorkProfileConfirmationHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!info.IsValid())
    return;

  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kNotRequired)) {
    return;
  }

  identity_manager_->RemoveObserver(this);
  SetUserImageURL(info.picture_url);
}

void WorkProfileConfirmationHandler::CloseModalSigninWindow(
    DiceTurnSyncOnHelper::SigninChoice result) {
  if (did_user_explicitly_interact_) {
    switch (result) {
      case DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL:
        base::RecordAction(
            base::UserMetricsAction("Signin_WorkProfilePrompt_Cancel"));
        break;
      case DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE:
        NOTREACHED();
        break;
      case DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE:
        base::RecordAction(
            base::UserMetricsAction("Signin_WorkProfilePrompt_Add"));
        break;
      case DiceTurnSyncOnHelper::SIGNIN_CHOICE_SIZE:
        NOTREACHED();
        break;
    }
  }
  std::move(callback_).Run(result);
  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
}

void WorkProfileConfirmationHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  AllowJavascript();

  if (browser_)
    signin::SetInitializedModalHeight(browser_, web_ui(), args);

  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kNotRequired));
  if (!primary_account_info) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }

  if (!primary_account_info->IsValid()) {
    identity_manager_->AddObserver(this);
  } else {
    SetUserImageURL(primary_account_info->picture_url);
  }
}
