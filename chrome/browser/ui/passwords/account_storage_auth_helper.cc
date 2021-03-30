// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace {
using ReauthSucceeded =
    password_manager::PasswordManagerClient::ReauthSucceeded;
}

AccountStorageAuthHelper::AccountStorageAuthHelper(
    Profile* profile,
    password_manager::PasswordFeatureManager* password_feature_manager)
    : profile_(profile),
      password_feature_manager_(password_feature_manager),
      signin_view_controller_getter_(base::BindRepeating(
          [](Profile* profile) -> SigninViewController* {
            if (Browser* browser = chrome::FindBrowserWithProfile(profile))
              return browser->signin_view_controller();
            return nullptr;
          },
          profile_)) {}

AccountStorageAuthHelper::~AccountStorageAuthHelper() = default;

void AccountStorageAuthHelper::TriggerOptInReauth(
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  SigninViewController* signin_view_controller =
      signin_view_controller_getter_.Run();
  if (!signin_view_controller) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  // In the rare case of concurrent requests, only consider the first one.
  if (reauth_abort_handle_) {
    std::move(reauth_callback).Run(ReauthSucceeded(false));
    return;
  }

  reauth_abort_handle_ = signin_view_controller->ShowReauthPrompt(
      primary_account_id, access_point,
      base::BindOnce(&AccountStorageAuthHelper::OnOptInReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(reauth_callback)));
#else
  std::move(reauth_callback).Run(ReauthSucceeded(false));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

void AccountStorageAuthHelper::TriggerSignIn(
    signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (SigninViewController* signin_controller =
          signin_view_controller_getter_.Run()) {
    signin_controller->ShowDiceAddAccountTab(access_point, std::string());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

void AccountStorageAuthHelper::OnOptInReauthCompleted(
    base::OnceCallback<void(ReauthSucceeded)> reauth_callback,
    signin::ReauthResult result) {
  reauth_abort_handle_.reset();

  bool succeeded = result == signin::ReauthResult::kSuccess;
  if (succeeded)
    password_feature_manager_->OptInToAccountStorage();
  std::move(reauth_callback).Run(ReauthSucceeded(succeeded));
}
