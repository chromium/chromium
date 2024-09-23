// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace {
using ReauthSucceeded =
    password_manager::PasswordManagerClient::ReauthSucceeded;
}

AccountStorageAuthHelper::AccountStorageAuthHelper(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    password_manager::PasswordFeatureManager* password_feature_manager,
    base::RepeatingCallback<SigninViewController*()>
        signin_view_controller_getter)
    : profile_(profile),
      identity_manager_(identity_manager),
      password_feature_manager_(password_feature_manager),
      signin_view_controller_getter_(std::move(signin_view_controller_getter)) {
  DCHECK(password_feature_manager_);
  DCHECK(signin_view_controller_getter_);
}

AccountStorageAuthHelper::~AccountStorageAuthHelper() = default;

void AccountStorageAuthHelper::TriggerOptInReauth(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
  // Reauth is only required if promos are allowed, see the predicate docs.
  CHECK(password_manager::features_util::AreAccountStorageOptInPromosAllowed());

  SigninViewController* signin_view_controller =
      signin_view_controller_getter_.Run();
  if (!signin_view_controller) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  if (!identity_manager_) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  CoreAccountId unconsented_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (unconsented_account_id.empty()) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  // In the rare case of concurrent requests, only consider the first one.
  if (reauth_abort_handle_) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  reauth_abort_handle_ = signin_view_controller->ShowReauthPrompt(
      unconsented_account_id, access_point,
      base::BindOnce(&AccountStorageAuthHelper::OnOptInReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(reauth_callback)));
}

void AccountStorageAuthHelper::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {
  signin_ui_util::ShowSigninPromptFromPromo(profile_, access_point);
}

void AccountStorageAuthHelper::OnOptInReauthCompleted(
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback,
    signin::ReauthResult result) {
  reauth_abort_handle_.reset();

  bool succeeded = result == signin::ReauthResult::kSuccess;
  if (succeeded) {
    password_feature_manager_->OptInToAccountStorage();
  }
  std::move(reauth_callback).Run(ReauthSucceeded(succeeded));
}
