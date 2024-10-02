// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

class ForcedProfileSwitchInterceptionHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  explicit ForcedProfileSwitchInterceptionHandle(
      base::OnceCallback<void(SigninInterceptionResult)> callback) {
    DCHECK(callback);
    cancelable_callback_.Reset(std::move(callback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(cancelable_callback_.callback(),
                                  SigninInterceptionResult::kAccepted));
  }
  ~ForcedProfileSwitchInterceptionHandle() override = default;

 private:
  base::CancelableOnceCallback<void(SigninInterceptionResult)>
      cancelable_callback_;
};

class OidcEnterpriseSigninInterceptionHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  OidcEnterpriseSigninInterceptionHandle(
      Browser* browser,
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      signin::SigninChoiceWithConfirmationCallback callback,
      base::OnceClosure dialog_closed_closure,
      base::OnceClosure retry_callback)
      : browser_(browser->AsWeakPtr()),
        bubble_parameters_(bubble_parameters),
        callback_(std::move(callback)) {
    DCHECK(browser_);
    DCHECK(callback_);
    CHECK(bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC);
    browser_->signin_view_controller()->ShowModalManagedUserNoticeDialog(
        std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
            bubble_parameters.intercepted_account,
            /*is_OIDC_account=*/true,
            /*profile_creation_required_by_policy=*/true,
            /*show_link_data_option=*/false,
            /*process_user_choice_callback=*/
            base::BindOnce(&OidcEnterpriseSigninInterceptionHandle::
                               OnEnterpriseInterceptionUserChoice,
                           weak_ptr_factory_.GetWeakPtr()),
            /*done_callback=*/
            base::BindOnce(&SigninViewController::CloseModalSignin,
                           browser_->signin_view_controller()->AsWeakPtr())
                .Then(std::move(dialog_closed_closure)),
            /*retry_callback=*/std::move(retry_callback)));
  }

  ~OidcEnterpriseSigninInterceptionHandle() override {
    if (browser_) {
      browser_->signin_view_controller()->CloseModalSignin();
    }
    if (callback_) {
      DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
          bubble_parameters_, browser_->profile(),
          SigninInterceptionResult::kDeclined);
      std::move(callback_).Run(signin::SIGNIN_CHOICE_CANCEL, base::DoNothing());
    }
  }

 private:
  void OnEnterpriseInterceptionUserChoice(
      signin::SigninChoice result,
      signin::SigninChoiceOperationDoneCallback done_callback) {
    SigninInterceptionResult interception_result =
        SigninInterceptionResult::kDeclined;
    switch (result) {
      case signin::SIGNIN_CHOICE_NEW_PROFILE:
        interception_result = SigninInterceptionResult::kAccepted;
        break;
      case signin::SIGNIN_CHOICE_CANCEL:
        interception_result = SigninInterceptionResult::kDeclined;
        break;
      case signin::SIGNIN_CHOICE_CONTINUE:
      case signin::SIGNIN_CHOICE_SIZE:
      default:
        NOTREACHED();
    }
    DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
        bubble_parameters_, browser_->profile(), interception_result);
    std::move(callback_).Run(result, std::move(done_callback));
  }

  base::WeakPtr<Browser> browser_;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  signin::SigninChoiceWithConfirmationCallback callback_;
  base::WeakPtrFactory<OidcEnterpriseSigninInterceptionHandle>
      weak_ptr_factory_{this};
};

class ForcedEnterpriseSigninInterceptionHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  ForcedEnterpriseSigninInterceptionHandle(
      Browser* browser,
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback)
      : browser_(browser->AsWeakPtr()),
        bubble_parameters_(bubble_parameters),
        profile_creation_required_by_policy_(
            bubble_parameters.interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced),
        show_link_data_option_(bubble_parameters.show_link_data_option),
        callback_(std::move(callback)) {
    DCHECK(browser_);
    DCHECK(callback_);
    browser_->signin_view_controller()->ShowModalManagedUserNoticeDialog(
        std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
            bubble_parameters.intercepted_account,
            /*is_OIDC_account=*/bubble_parameters.interception_type ==
                WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC,
            profile_creation_required_by_policy_, show_link_data_option_,
            /*process_user_choice_callback=*/
            base::BindOnce(&ForcedEnterpriseSigninInterceptionHandle::
                               OnEnterpriseInterceptionDialogClosed,
                           weak_ptr_factory_.GetWeakPtr()),
            /*done_callback=*/
            base::BindOnce(&SigninViewController::CloseModalSignin,
                           browser_->signin_view_controller()->AsWeakPtr())));
  }

  ~ForcedEnterpriseSigninInterceptionHandle() override {
    if (!browser_) {
      return;
    }
    browser_->signin_view_controller()->CloseModalSignin();
    if (callback_) {
      DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
          bubble_parameters_, browser_->profile(),
          SigninInterceptionResult::kDeclined);
      std::move(callback_).Run(SigninInterceptionResult::kDeclined);
    }
  }

 private:
  void OnEnterpriseInterceptionDialogClosed(signin::SigninChoice result) {
    SigninInterceptionResult interception_result =
        SigninInterceptionResult::kDeclined;
    switch (result) {
      case signin::SIGNIN_CHOICE_NEW_PROFILE:
        interception_result = SigninInterceptionResult::kAccepted;
        break;
      case signin::SIGNIN_CHOICE_CONTINUE:
        DCHECK(!profile_creation_required_by_policy_ || show_link_data_option_);
        interception_result =
            SigninInterceptionResult::kAcceptedWithExistingProfile;
        break;
      case signin::SIGNIN_CHOICE_CANCEL:
        interception_result = SigninInterceptionResult::kDeclined;
        break;
      case signin::SIGNIN_CHOICE_SIZE:
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
        bubble_parameters_, browser_->profile(), interception_result);
    std::move(callback_).Run(interception_result);
  }

  base::WeakPtr<Browser> browser_;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  const bool profile_creation_required_by_policy_;
  const bool show_link_data_option_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;
  base::WeakPtrFactory<ForcedEnterpriseSigninInterceptionHandle>
      weak_ptr_factory_{this};
};

}  // namespace

DiceWebSigninInterceptorDelegate::DiceWebSigninInterceptorDelegate() = default;

DiceWebSigninInterceptorDelegate::~DiceWebSigninInterceptorDelegate() = default;

bool DiceWebSigninInterceptorDelegate::IsSigninInterceptionSupported(
    const content::WebContents& web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(&web_contents);
  // The profile creation flow has no browser.
  if (!browser) {
    return false;
  }
  return IsSigninInterceptionSupportedInternal(*browser);
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble(
    content::WebContents* web_contents,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  CHECK_NE(bubble_parameters.interception_type,
           WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC);
  if (bubble_parameters.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced) {
    return std::make_unique<ForcedProfileSwitchInterceptionHandle>(
        std::move(callback));
  }

  if (!web_contents) {
    std::move(callback).Run(SigninInterceptionResult::kNotDisplayed);
    return nullptr;
  }

  if (bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced ||
      bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::
              kEnterpriseAcceptManagement) {
    return std::make_unique<ForcedEnterpriseSigninInterceptionHandle>(
        chrome::FindBrowserWithTab(web_contents), bubble_parameters,
        std::move(callback));
  }

  return ShowSigninInterceptionBubbleInternal(
      chrome::FindBrowserWithTab(web_contents), bubble_parameters,
      std::move(callback));
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowOidcInterceptionDialog(
    content::WebContents* web_contents,
    const DiceWebSigninInterceptorDelegate::BubbleParameters& bubble_parameters,
    signin::SigninChoiceWithConfirmationCallback callback,
    base::OnceClosure dialog_closed_closure,
    base::OnceClosure retry_callback) {
  CHECK_EQ(bubble_parameters.interception_type,
           WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC);
  return std::make_unique<OidcEnterpriseSigninInterceptionHandle>(
      chrome::FindBrowserWithTab(web_contents), bubble_parameters,
      std::move(callback), std::move(dialog_closed_closure),
      std::move(retry_callback));
}

void DiceWebSigninInterceptorDelegate::ShowFirstRunExperienceInNewProfile(
    Browser* browser,
    const CoreAccountId& account_id,
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  browser->signin_view_controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id,
      interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
}

// static
std::string DiceWebSigninInterceptorDelegate::GetHistogramSuffix(
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  switch (interception_type) {
    case WebSigninInterceptor::SigninInterceptionType::kEnterprise:
    case WebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
      return ".Enterprise";
    case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
      return ".MultiUser";
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      return ".Switch";
    case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
      return ".ChromeSignin";
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
      return ".EnterpriseForced";
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC:
      return ".EnterpriseOIDC";
  }
}

// static
void DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    Profile* profile,
    SigninInterceptionResult result) {
  std::string histogram_base_name =
      base::StrCat({"Signin.InterceptResult",
                    GetHistogramSuffix(bubble_parameters.interception_type)});
  // Record aggregated histogram for each interception type.
  base::UmaHistogramEnumeration(histogram_base_name, result);

  // Record histogram for users with ClearOnExit set for the Chrome Signin
  // Bubble.
  if (bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kChromeSignin &&
      ChromeSigninClientFactory::GetForProfile(profile)
          ->AreSigninCookiesDeletedOnExit()) {
    base::UmaHistogramEnumeration(
        base::StrCat({histogram_base_name, ".CookiesDeletedOnExit"}), result);
  }

  // Record histogram sliced by Sync.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string sync_suffix =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)
          ? ".Sync"
          : ".NoSync";
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_base_name, sync_suffix}), result);
  // Record Signin Pending status.
  if (signin_util::IsSigninPending(identity_manager)) {
    base::UmaHistogramEnumeration(
        base::StrCat({histogram_base_name, ".SigninPending"}), result);
  }

  // For Enterprise, slice per enterprise status for each account.
  if (bubble_parameters.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kEnterprise) {
    if (bubble_parameters.intercepted_account.IsManaged()) {
      base::UmaHistogramEnumeration(
          base::StrCat({histogram_base_name, ".NewIsEnterprise"}), result);
    }
    if (bubble_parameters.primary_account.IsManaged()) {
      base::UmaHistogramEnumeration(
          base::StrCat({histogram_base_name, ".PrimaryIsEnterprise"}), result);
    }
  }
}
