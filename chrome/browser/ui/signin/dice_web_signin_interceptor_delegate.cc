// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

class ForcedProfileSwitchInterceptionHandle
    : public ScopedDiceWebSigninInterceptionBubbleHandle {
 public:
  explicit ForcedProfileSwitchInterceptionHandle(
      base::OnceCallback<void(SigninInterceptionResult)> callback) {
    DCHECK(callback);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  SigninInterceptionResult::kAccepted));
  }
  ~ForcedProfileSwitchInterceptionHandle() override = default;
};

class ForcedEnterpriseSigninInterceptionHandle
    : public ScopedDiceWebSigninInterceptionBubbleHandle {
 public:
  ForcedEnterpriseSigninInterceptionHandle(
      Browser* browser,
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback)
      : browser_(browser),
        profile_creation_required_by_policy_(
            bubble_parameters.interception_type ==
            DiceWebSigninInterceptor::SigninInterceptionType::
                kEnterpriseForced),
        show_link_data_option_(bubble_parameters.show_link_data_option),
        callback_(std::move(callback)) {
    DCHECK(browser_);
    DCHECK(callback_);
    ShowEnterpriseProfileInterceptionDialog(
        bubble_parameters.intercepted_account,
        bubble_parameters.profile_highlight_color);
  }

  ~ForcedEnterpriseSigninInterceptionHandle() override {
    browser_->signin_view_controller()->CloseModalSignin();
    if (callback_)
      std::move(callback_).Run(SigninInterceptionResult::kDeclined);
  }

 private:
  void ShowEnterpriseProfileInterceptionDialog(const AccountInfo& account_info,
                                               SkColor profile_color) {
    browser_->signin_view_controller()->ShowModalEnterpriseConfirmationDialog(
        account_info, profile_creation_required_by_policy_,
        show_link_data_option_, profile_color,
        base::BindOnce(&ForcedEnterpriseSigninInterceptionHandle::
                           OnEnterpriseInterceptionDialogClosed,
                       base::Unretained(this)));
  }

  void OnEnterpriseInterceptionDialogClosed(signin::SigninChoice result) {
    switch (result) {
      case signin::SIGNIN_CHOICE_NEW_PROFILE:
        std::move(callback_).Run(SigninInterceptionResult::kAccepted);
        break;
      case signin::SIGNIN_CHOICE_CONTINUE:
        DCHECK(!profile_creation_required_by_policy_ || show_link_data_option_);
        std::move(callback_).Run(
            SigninInterceptionResult::kAcceptedWithExistingProfile);
        break;
      case signin::SIGNIN_CHOICE_CANCEL:
        std::move(callback_).Run(SigninInterceptionResult::kDeclined);
        break;
      case signin::SIGNIN_CHOICE_SIZE:
      default:
        NOTREACHED();
        break;
    }
  }

  raw_ptr<Browser> browser_;
  const bool profile_creation_required_by_policy_;
  const bool show_link_data_option_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;
};

}  // namespace

DiceWebSigninInterceptorDelegate::DiceWebSigninInterceptorDelegate() = default;

DiceWebSigninInterceptorDelegate::~DiceWebSigninInterceptorDelegate() = default;

std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble(
    content::WebContents* web_contents,
    const BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  if (bubble_parameters.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced) {
    return std::make_unique<ForcedProfileSwitchInterceptionHandle>(
        std::move(callback));
  }

  if (!web_contents) {
    std::move(callback).Run(SigninInterceptionResult::kNotDisplayed);
    return nullptr;
  }

  if (bubble_parameters.interception_type ==
          DiceWebSigninInterceptor::SigninInterceptionType::kEnterpriseForced ||
      bubble_parameters.interception_type ==
          DiceWebSigninInterceptor::SigninInterceptionType::
              kEnterpriseAcceptManagement) {
    return std::make_unique<ForcedEnterpriseSigninInterceptionHandle>(
        chrome::FindBrowserWithWebContents(web_contents), bubble_parameters,
        std::move(callback));
  }

  return ShowSigninInterceptionBubbleInternal(
      chrome::FindBrowserWithWebContents(web_contents), bubble_parameters,
      std::move(callback));
}

void DiceWebSigninInterceptorDelegate::ShowFirstRunExperienceInNewProfile(
    Browser* browser,
    const CoreAccountId& account_id,
    DiceWebSigninInterceptor::SigninInterceptionType interception_type) {
  if (base::FeatureList::IsEnabled(kSyncPromoAfterSigninIntercept)) {
    browser->signin_view_controller()
        ->ShowModalInterceptFirstRunExperienceDialog(
            account_id, interception_type ==
                            DiceWebSigninInterceptor::SigninInterceptionType::
                                kEnterpriseForced);
  } else {
    // Don't show the customization bubble if a valid policy theme is set.
    if (ThemeServiceFactory::GetForProfile(browser->profile())
            ->UsingPolicyTheme()) {
      // Show the profile switch IPH that is normally shown after the
      // customization bubble.
      browser->window()->MaybeShowProfileSwitchIPH();
      return;
    }
    ShowProfileCustomizationBubbleInternal(browser);
  }
}
