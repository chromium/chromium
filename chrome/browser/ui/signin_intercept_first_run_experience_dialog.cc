// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_intercept_first_run_experience_dialog.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// Delegate class for DiceTurnSyncOnHelper. Determines what will be the next
// step for the first run based on Sync availabitily.
class SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate
    : public DiceTurnSyncOnHelper::Delegate,
      public LoginUIService::Observer {
 public:
  explicit InterceptTurnSyncOnHelperDelegate(
      base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog);
  ~InterceptTurnSyncOnHelperDelegate() override;

  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

 private:
  base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog_;

  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
};

SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate::
    InterceptTurnSyncOnHelperDelegate(
        base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog)
    : dialog_(std::move(dialog)) {}
SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate::
    ~InterceptTurnSyncOnHelperDelegate() = default;

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowLoginError(
        const SigninUIError& error) {
  // Do not display the sync error since the user hasn't asked for sync
  // explicitly. Skip to the next step.
  if (dialog_)
    dialog_->DoNextStep(Step::kTurnOnSync, Step::kProfileCustomization);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
        const std::string& previous_email,
        const std::string& new_email,
        DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  NOTREACHED() << "Sign-in intercept shouldn't create a profile for an "
                  "account known to Chrome";
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
        const AccountInfo& account_info,
        DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  // This is a brand new profile. Skip the enterprise confirmation.
  // TODO(crbug.com/1282157): Do not show the sync promo if either
  // - PromotionalTabsEnabled policy is set to False, or
  // - the user went through the Profile Separation dialog.
  std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncConfirmation(
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) {
  if (!dialog_) {
    std::move(callback).Run(LoginUIService::ABORT_SYNC);
    return;
  }

  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(dialog_->browser_->profile()));
  DCHECK(!sync_confirmation_callback_);
  sync_confirmation_callback_ = std::move(callback);
  dialog_->DoNextStep(Step::kTurnOnSync, Step::kSyncConfirmation);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncDisabledConfirmation(
        bool is_managed_account,
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) {
  // Abort the sync flow and proceed to profile customization.
  if (dialog_) {
    dialog_->DoNextStep(Step::kTurnOnSync, Step::kProfileCustomization);
  }

  // `SYNC_WITH_DEFAULT_SETTINGS` for the sync disable confirmation means "stay
  // signed in". See https://crbug.com/1141341.
  std::move(callback).Run(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncSettings() {
  if (dialog_) {
    // Dialog's step is updated in OnSyncConfirmationUIClosed(). This
    // function only needs to open the Sync Settings.
    DCHECK_EQ(dialog_->current_step_, Step::kSyncConfirmation);
    chrome::ShowSettingsSubPage(dialog_->browser_, chrome::kSyncSetupSubPage);
  }
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::SwitchToProfile(Profile* new_profile) {
  NOTREACHED() << "Sign-in intercept shouldn't create a new profile for an "
                  "account known to Chrome";
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::OnSyncConfirmationUIClosed(
        LoginUIService::SyncConfirmationUIClosedResult result) {
  scoped_login_ui_service_observation_.Reset();

  Step next_step;
  switch (result) {
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
    case LoginUIService::ABORT_SYNC:
      next_step = Step::kProfileCustomization;
      break;
    case LoginUIService::UI_CLOSED:
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      next_step = Step::kProfileSwitchIPHAndCloseModal;
      break;
  }

  if (result == LoginUIService::UI_CLOSED) {
    // Sync must be aborted if the user didn't interact explicitly with the
    // dialog.
    result = LoginUIService::ABORT_SYNC;
  }

  // Save a local reference to `dialog_` before `this` is destroyed.
  auto local_dialog = dialog_;

  // Run the callback before moving to the next step to give
  // `DiceTurnSyncOnHelper` the last opportunity to call `dialog_`'s methods.
  // This is important for `ShowSyncSettings()`, for example.
  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
  // `this` may now be deleted.

  if (local_dialog) {
    local_dialog->DoNextStep(Step::kSyncConfirmation, next_step);
  }
}

SigninInterceptFirstRunExperienceDialog::
    SigninInterceptFirstRunExperienceDialog(Browser* browser,
                                            const CoreAccountId& account_id,
                                            base::OnceClosure on_close_callback)
    : SigninModalDialog(std::move(on_close_callback)),
      browser_(browser),
      account_id_(account_id) {
  DoNextStep(Step::kStart, Step::kTurnOnSync);
}

SigninInterceptFirstRunExperienceDialog::
    ~SigninInterceptFirstRunExperienceDialog() = default;

void SigninInterceptFirstRunExperienceDialog::CloseModalDialog() {
  if (dialog_delegate_) {
    // Delegate will notify `this` when modal signin is closed.
    dialog_delegate_->CloseModalSignin();
  } else {
    // No dialog is displayed yet, so close `this` directly.
    OnModalDialogClosed();
  }
}
void SigninInterceptFirstRunExperienceDialog::ResizeNativeView(int height) {
  DCHECK(dialog_delegate_);
  dialog_delegate_->ResizeNativeView(height);
}

content::WebContents*
SigninInterceptFirstRunExperienceDialog::GetModalDialogWebContentsForTesting() {
  return dialog_delegate_ ? dialog_delegate_->GetWebContents() : nullptr;
}

void SigninInterceptFirstRunExperienceDialog::OnModalDialogClosed() {
  DCHECK(!dialog_delegate_ ||
         dialog_delegate_observation_.IsObservingSource(dialog_delegate_));
  dialog_delegate_observation_.Reset();
  dialog_delegate_ = nullptr;
  NotifyModalDialogClosed();
}

void SigninInterceptFirstRunExperienceDialog::DoNextStep(
    Step expected_current_step,
    Step step) {
  DCHECK_EQ(expected_current_step, current_step_);
  // Going to a previous step is not allowed.
  DCHECK_GT(step, current_step_);
  current_step_ = step;

  switch (step) {
    case Step::kStart:
      NOTREACHED();
      return;
    case Step::kTurnOnSync:
      DoTurnOnSync();
      return;
    case Step::kSyncConfirmation:
      DoSyncConfirmation();
      return;
    case Step::kProfileCustomization:
      DoProfileCustomization();
      return;
    case Step::kProfileSwitchIPHAndCloseModal:
      DoProfileSwitchIPHAndCloseModal();
      return;
  }
}

void SigninInterceptFirstRunExperienceDialog::DoTurnOnSync() {
  // DiceTurnSyncOnHelper deletes itself once done.
  new DiceTurnSyncOnHelper(
      browser_->profile(),
      signin_metrics::AccessPoint::
          ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kSigninPrimaryAccount, account_id_,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<InterceptTurnSyncOnHelperDelegate>(
          weak_ptr_factory_.GetWeakPtr()),
      base::OnceClosure());
}

void SigninInterceptFirstRunExperienceDialog::DoSyncConfirmation() {
  SetDialogDelegate(
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(browser_));
  PreloadProfileCustomizationUI();
}

void SigninInterceptFirstRunExperienceDialog::DoProfileCustomization() {
  // Don't show the customization bubble if a valid policy theme is set.
  if (ThemeServiceFactory::GetForProfile(browser_->profile())
          ->UsingPolicyTheme()) {
    // Show the profile switch IPH that is normally shown after the
    // customization bubble.
    DoNextStep(Step::kProfileCustomization,
               Step::kProfileSwitchIPHAndCloseModal);
    return;
  }

  if (!dialog_delegate_) {
    // Modal dialog doesn't exist yet, create a new one.
    SetDialogDelegate(
        SigninViewControllerDelegate::CreateProfileCustomizationDelegate(
            browser_));
    return;
  }

  DCHECK(profile_customization_preloaded_contents_);
  dialog_delegate_->SetWebContents(
      profile_customization_preloaded_contents_.get());
}

void SigninInterceptFirstRunExperienceDialog::
    DoProfileSwitchIPHAndCloseModal() {
  browser_->window()->MaybeShowProfileSwitchIPH();
  CloseModalDialog();
}

void SigninInterceptFirstRunExperienceDialog::SetDialogDelegate(
    SigninViewControllerDelegate* delegate) {
  DCHECK(!dialog_delegate_);
  DCHECK(!dialog_delegate_observation_.IsObserving());
  dialog_delegate_ = delegate;
  dialog_delegate_observation_.Observe(dialog_delegate_);
}

void SigninInterceptFirstRunExperienceDialog::PreloadProfileCustomizationUI() {
  profile_customization_preloaded_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          browser_->profile(),
          content::SiteInstance::Create(browser_->profile())));
  profile_customization_preloaded_contents_->GetController().LoadURL(
      GURL(chrome::kChromeUIProfileCustomizationURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  ProfileCustomizationUI* web_ui =
      profile_customization_preloaded_contents_->GetWebUI()
          ->GetController()
          ->GetAs<ProfileCustomizationUI>();
  DCHECK(web_ui);
  web_ui->Initialize(
      base::BindOnce(&SigninInterceptFirstRunExperienceDialog::
                         OnProfileCustomizationDoneButtonClicked,
                     // Unretained is fine because `this` owns the web contents.
                     base::Unretained(this)));
}

void SigninInterceptFirstRunExperienceDialog::
    OnProfileCustomizationDoneButtonClicked() {
  DoNextStep(Step::kProfileCustomization, Step::kProfileSwitchIPHAndCloseModal);
}
