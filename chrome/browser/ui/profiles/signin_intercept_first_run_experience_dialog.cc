// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/signin_intercept_first_run_experience_dialog.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_customization_synced_theme_waiter.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace {

void RecordDialogEvent(
    SigninInterceptFirstRunExperienceDialog::DialogEvent event) {
  base::UmaHistogramEnumeration("Signin.Intercept.FRE.Event", event);
}

}  // namespace

// Delegate class for the History Sync Optin Helper.
class SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  explicit InterceptHistorySyncOptinHelperDelegate(
      base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog);
  ~InterceptHistorySyncOptinHelperDelegate() override;

  // HistorySyncOptinHelper::Delegate:
  void ShowHistorySyncOptinScreen(
      Profile* profile,
      HistorySyncOptinHelper::FlowCompletedCallback callback) override;
  void ShowAccountManagementScreen(
      signin::SigninChoiceCallback on_account_management_screen_closed)
      override;
  void FinishFlowWithoutHistorySyncOptin() override;

  HistorySyncOptinHelper::FlowCompletedCallback
  MoveHistorySyncOptinCompletionCallback();

  base::WeakPtr<SigninInterceptFirstRunExperienceDialog::
                    InterceptHistorySyncOptinHelperDelegate>
  GetWeakPtr();

 private:
  void OnHistorySyncOptinStepFinished(
      Step current_step,
      HistorySyncOptinHelper::ScreenChoiceResult screen_choice_result);

  const base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog_;

  HistorySyncOptinHelper::FlowCompletedCallback
      history_sync_optin_completion_callback_;

  base::WeakPtrFactory<InterceptHistorySyncOptinHelperDelegate>
      weak_ptr_factory_{this};
};

SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::
        InterceptHistorySyncOptinHelperDelegate(
            base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog)
    : dialog_(std::move(dialog)) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));
  CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
}

SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::
        ~InterceptHistorySyncOptinHelperDelegate() = default;

void SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::ShowAccountManagementScreen(
        signin::SigninChoiceCallback on_account_management_screen_closed) {
  // This flow marks the management as approved and does not invoke this
  // method.
  NOTREACHED();
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::
        FinishFlowWithoutHistorySyncOptin() {
  OnHistorySyncOptinStepFinished(
      Step::kStartHistorySyncOptin,
      HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::ShowHistorySyncOptinScreen(
        Profile* profile,
        HistorySyncOptinHelper::FlowCompletedCallback
            history_sync_optin_flow_completed_callback) {
  if (!dialog_) {
    std::move(history_sync_optin_flow_completed_callback.value())
        .Run(HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
    return;
  }
  CHECK(profile);
  CHECK(!history_sync_optin_flow_completed_callback->is_null());

  PrefService* local_state = g_browser_process->local_state();
  if (dialog_->is_forced_intercept_ ||
      (local_state && !local_state->GetBoolean(prefs::kPromotionsEnabled))) {
    // Don't show the history sync promo if
    // - the user went through the forced interception, or
    // - promotional tabs, or promotions in general, are disabled by policy.
    // Executing this callback moves the current flow to the next `Step`.
    std::move(history_sync_optin_flow_completed_callback.value())
        .Run(HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
    OnHistorySyncOptinStepFinished(
        Step::kStartHistorySyncOptin,
        HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
    return;
  }

  auto do_next_step_callback =
      HistorySyncOptinHelper::FlowCompletedCallback(base::BindOnce(
          &SigninInterceptFirstRunExperienceDialog::
              InterceptHistorySyncOptinHelperDelegate::
                  OnHistorySyncOptinStepFinished,
          weak_ptr_factory_.GetWeakPtr(), Step::kShowHistorySyncScreen));

  auto record_history_sync_optin_choice_callback =
      HistorySyncOptinHelper::FlowCompletedCallback(base::BindOnce(
          [](HistorySyncOptinHelper::ScreenChoiceResult screen_choice_result) {
            switch (screen_choice_result) {
              case HistorySyncOptinHelper::ScreenChoiceResult::kAccepted:
                RecordDialogEvent(SigninInterceptFirstRunExperienceDialog::
                                      DialogEvent::kHistorySyncOptinAccept);
                break;
              case HistorySyncOptinHelper::ScreenChoiceResult::kDeclined:
                RecordDialogEvent(SigninInterceptFirstRunExperienceDialog::
                                      DialogEvent::kHistorySyncOptinReject);
                break;
              case HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped:
              case HistorySyncOptinHelper::ScreenChoiceResult::kDismissed:
                break;
            }
          }));

  std::vector<HistorySyncOptinHelper::FlowCompletedCallback> callbacks;
  callbacks.emplace_back(std::move(history_sync_optin_flow_completed_callback));
  callbacks.emplace_back(std::move(record_history_sync_optin_choice_callback));
  callbacks.emplace_back(std::move(do_next_step_callback));

  history_sync_optin_completion_callback_ =
      CombineCallbacks<HistorySyncOptinHelper::FlowCompletedCallback,
                       HistorySyncOptinHelper::ScreenChoiceResult>(
          std::move(callbacks));
  dialog_->DoNextStep(Step::kStartHistorySyncOptin,
                      Step::kShowHistorySyncScreen);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::OnHistorySyncOptinStepFinished(
        Step current_step,
        HistorySyncOptinHelper::ScreenChoiceResult screen_choice_result) {
  Step next_step;
  switch (screen_choice_result) {
    case HistorySyncOptinHelper::ScreenChoiceResult::kAccepted:
    case HistorySyncOptinHelper::ScreenChoiceResult::kDeclined:
    case HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped:
      next_step = Step::kWaitForSyncedTheme;
      break;
    case HistorySyncOptinHelper::ScreenChoiceResult::kDismissed:
      next_step = Step::kProfileSwitchIPHAndCloseModal;
      break;
  }
  if (dialog_) {
    dialog_->DoNextStep(current_step, next_step);
  }
}

HistorySyncOptinHelper::FlowCompletedCallback
SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::
        MoveHistorySyncOptinCompletionCallback() {
  CHECK(!history_sync_optin_completion_callback_->is_null());
  return std::move(history_sync_optin_completion_callback_);
}

base::WeakPtr<SigninInterceptFirstRunExperienceDialog::
                  InterceptHistorySyncOptinHelperDelegate>
SigninInterceptFirstRunExperienceDialog::
    InterceptHistorySyncOptinHelperDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// Delegate class for TurnSyncOnHelper. Determines what will be the next
// step for the first run based on Sync availabitily.
class SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate
    : public TurnSyncOnHelper::Delegate,
      public LoginUIService::Observer {
 public:
  explicit InterceptTurnSyncOnHelperDelegate(
      base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog);
  ~InterceptTurnSyncOnHelperDelegate() override;

  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  bool ShouldAbortBeforeShowSyncDisabledConfirmation() override;
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
  const base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog_;
  // Store `browser_` separately as it may outlive `dialog_`.
  const base::WeakPtr<Browser> browser_;

  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
};

SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate::
    InterceptTurnSyncOnHelperDelegate(
        base::WeakPtr<SigninInterceptFirstRunExperienceDialog> dialog)
    : dialog_(std::move(dialog)), browser_(dialog_->browser_->AsWeakPtr()) {
  CHECK(!base::FeatureList::IsEnabled(
      syncer::kReplaceSyncPromosWithSignInPromos));
}

SigninInterceptFirstRunExperienceDialog::InterceptTurnSyncOnHelperDelegate::
    ~InterceptTurnSyncOnHelperDelegate() = default;

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowLoginError(
        const SigninUIError& error) {
  // Do not display the sync error since the user hasn't asked for sync
  // explicitly. Skip to the next step.
  if (dialog_) {
    dialog_->DoNextStep(Step::kTurnOnSync, Step::kProfileCustomization);
  }
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
        const std::string& previous_email,
        const std::string& new_email,
        signin::SigninChoiceCallback callback) {
  NOTREACHED() << "Sign-in intercept shouldn't create a profile for an "
                  "account known to Chrome";
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
        const AccountInfo& account_info,
        signin::SigninChoiceCallback callback) {
  // This is a brand new profile. Skip the enterprise confirmation.
  // TODO(crbug.com/40209493): Do not show the sync promo if
  // PromotionsEnabled policy is set to False
  std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncConfirmation(
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) {
  if (!dialog_) {
    std::move(callback).Run(LoginUIService::ABORT_SYNC);
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  if (dialog_->is_forced_intercept_ ||
      (local_state && !local_state->GetBoolean(prefs::kPromotionsEnabled))) {
    // Don't show the sync promo if
    // - the user went through the forced interception, or
    // - promotional tabs, or promotions in general, are disabled by policy.
    dialog_->DoNextStep(Step::kTurnOnSync, Step::kProfileCustomization);
    std::move(callback).Run(LoginUIService::ABORT_SYNC);
    return;
  }

  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(browser_->profile()));
  DCHECK(!sync_confirmation_callback_);
  sync_confirmation_callback_ = std::move(callback);
  dialog_->DoNextStep(Step::kTurnOnSync, Step::kSyncConfirmation);
}

bool SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::
        ShouldAbortBeforeShowSyncDisabledConfirmation() {
  // Abort the sync flow and proceed to profile customization.
  if (dialog_) {
    dialog_->DoNextStep(Step::kTurnOnSync, Step::kProfileCustomization);
  }

  return true;
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncDisabledConfirmation(
        bool is_managed_account,
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) {
  // If Sync is disabled, the `TurnSyncOnHelper` should quit earlier due to
  // `ShouldAbortBeforeShowSyncDisabledConfirmation()`.
  NOTREACHED();
}

void SigninInterceptFirstRunExperienceDialog::
    InterceptTurnSyncOnHelperDelegate::ShowSyncSettings() {
  // Dialog's step is updated in OnSyncConfirmationUIClosed(). This
  // function only needs to open the Sync Settings.
  if (browser_) {
    chrome::ShowSettingsSubPage(browser_.get(), chrome::kSyncSetupSubPage);
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
      RecordDialogEvent(DialogEvent::kSyncConfirmationClickConfirm);
      next_step = Step::kWaitForSyncedTheme;
      break;
    case LoginUIService::ABORT_SYNC:
      RecordDialogEvent(DialogEvent::kSyncConfirmationClickCancel);
      next_step = Step::kProfileCustomization;
      break;
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      RecordDialogEvent(DialogEvent::kSyncConfirmationClickSettings);
      [[fallthrough]];
    case LoginUIService::UI_CLOSED:
      next_step = Step::kProfileSwitchIPHAndCloseModal;
      break;
  }

  // This may delete `dialog_`.
  if (dialog_) {
    dialog_->DoNextStep(Step::kSyncConfirmation, next_step);
  }

  if (result == LoginUIService::UI_CLOSED) {
    // Sync must be aborted if the user didn't interact explicitly with the
    // dialog.
    result = LoginUIService::ABORT_SYNC;
  }

  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
  // `this` may now be deleted.
}

SigninInterceptFirstRunExperienceDialog::
    SigninInterceptFirstRunExperienceDialog(Browser* browser,
                                            const CoreAccountId& account_id,
                                            bool is_forced_intercept,
                                            base::OnceClosure on_close_callback)
    : SigninModalDialog(std::move(on_close_callback)),
      browser_(browser),
      account_id_(account_id),
      is_forced_intercept_(is_forced_intercept) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // This is a brand new profile. There should be enterprise confirmation
    // screens offering the option to create another profile. This class handles
    // management.
    auto_accept_management_ = enterprise_util::
        EnabledAutomaticManagementDisclaimerAcceptanceUntilReset(
            browser_->profile());

    if (!base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer)) {
      // Trigger the disclaimer service is to auto-approve the management and
      // fetch the applicable policies.
      auto* profile_management_disclaimer_service =
          ProfileManagementDisclaimerServiceFactory::GetForProfile(
              browser_->profile());
      CHECK(profile_management_disclaimer_service);
      const CoreAccountId account_id_of_ongoing_management_flow =
          profile_management_disclaimer_service
              ->GetAccountBeingConsideredForManagementIfAny();
      // If another account is being made managed then we cannot invoke
      // `EnsureManagedProfileForAccount` for the present account. However for a
      // newly created profile this case should not happen.
      // TODO(crbug.com/418143300): Maybe track this case with a metric.
      if (account_id_of_ongoing_management_flow.empty() ||
          account_id_of_ongoing_management_flow == account_id_) {
        profile_management_disclaimer_service->EnsureManagedProfileForAccount(
            account_id_,
            signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience,
            base::DoNothing());
      }
    }
  }
}

void SigninInterceptFirstRunExperienceDialog::Show() {
  RecordDialogEvent(DialogEvent::kStart);
  Step next_step =
      base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) &&
              base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)
          ? Step::kStartHistorySyncOptin
          : Step::kTurnOnSync;
  DoNextStep(Step::kStart, next_step);
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
    case Step::kTurnOnSync:
      if (base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)) {
        // TODO(crbug.com/418143300): Until we implement the proper flow
        // (flag `kUnoPhase2FollowUp`) for the History Sync optin screen,
        // skip entirely the replaced steps
        // Step::kTurnOnSync and Step::kSyncConfirmation.
        DoNextStep(Step::kTurnOnSync, Step::kWaitForSyncedTheme);
        return;
      }
      DoTurnOnSync();
      return;
    case Step::kSyncConfirmation:
      if (base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)) {
        // TODO(crbug.com/418143300): Until we implement the proper flow for the
        // History Sync optin screen, skip entirely the replaced steps
        // Step::kTurnOnSync and Step::kSyncConfirmation.
        NOTREACHED();
      }
      DoSyncConfirmation();
      return;
    case Step::kStartHistorySyncOptin:
      CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
      CHECK(base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos));
      DoStartHistorySync();
      return;
    case Step::kShowHistorySyncScreen:
      CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
      DoShowHistorySyncOptin();
      return;
    case Step::kWaitForSyncedTheme:
      DoWaitForSyncedTheme();
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
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience;
  const signin_metrics::PromoAction promo_action =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  signin_metrics::LogSigninAccessPointStarted(access_point, promo_action);
  signin_metrics::RecordSigninUserActionForAccessPoint(access_point);

  // TurnSyncOnHelper deletes itself once done.
  new TurnSyncOnHelper(browser_->profile(), access_point, promo_action,
                       account_id_,
                       TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                       std::make_unique<InterceptTurnSyncOnHelperDelegate>(
                           weak_ptr_factory_.GetWeakPtr()),
                       base::OnceClosure());
}

void SigninInterceptFirstRunExperienceDialog::DoSyncConfirmation() {
  RecordDialogEvent(DialogEvent::kShowSyncConfirmation);
  SetDialogDelegate(
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
          browser_, SyncConfirmationStyle::kSigninInterceptModal,
          /*is_sync_promo=*/true));
  PreloadProfileCustomizationUI();
}

void SigninInterceptFirstRunExperienceDialog::DoStartHistorySync() {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  CHECK(identity_manager);
  auto extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id_);
  auto history_sync_optin_delegate =
      std::make_unique<InterceptHistorySyncOptinHelperDelegate>(
          weak_ptr_factory_.GetWeakPtr());
  history_sync_optin_delegate_ = history_sync_optin_delegate->GetWeakPtr();
  auto* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(browser_->profile());
  CHECK(history_sync_optin_service);
  history_sync_optin_service->StartHistorySyncOptinFlow(
      extended_account_info, std::move(history_sync_optin_delegate),
      signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience);
}

void SigninInterceptFirstRunExperienceDialog::DoShowHistorySyncOptin() {
  RecordDialogEvent(DialogEvent::kShowHistorySyncOptinScreen);
  CHECK(history_sync_optin_delegate_);
  SetDialogDelegate(
      SigninViewControllerDelegate::CreateSyncHistoryOptInDelegate(
          browser_, /*should_close_modal_dialog=*/false,
          HistorySyncOptinLaunchContext::kModal,
          history_sync_optin_delegate_
              ->MoveHistorySyncOptinCompletionCallback()));
  PreloadProfileCustomizationUI();
}

void SigninInterceptFirstRunExperienceDialog::DoWaitForSyncedTheme() {
  synced_theme_waiter_ =
      std::make_unique<ProfileCustomizationSyncedThemeWaiter>(
          SyncServiceFactory::GetForProfile(browser_->profile()),
          ThemeServiceFactory::GetForProfile(browser_->profile()),
          base::BindOnce(
              &SigninInterceptFirstRunExperienceDialog::OnSyncedThemeReady,
              // Unretained() is fine because `this` owns `synced_theme_waiter_`
              base::Unretained(this)));
  synced_theme_waiter_->Run();
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

  RecordDialogEvent(DialogEvent::kShowProfileCustomization);
  if (!dialog_delegate_) {
    // Modal dialog doesn't exist yet, create a new one.
    // TODO(crbug.com/40241939): Add a callback for handling customization
    // result in
    // `SigninViewControllerDelegate::CreateProfileCustomizationDelegate()` and
    // pass it to `ProfileCustomizationUI::Initialize()`.
    SetDialogDelegate(
        SigninViewControllerDelegate::CreateProfileCustomizationDelegate(
            browser_, /*is_local_profile_creation=*/false,
            /*show_profile_switch_iph=*/true,
            // TODO(crbug.com/351333491) Clarify if we want the IPH when a new
            // profile is created following a context-signin.
            /*show_supervised_user_iph=*/false));
    return;
  }

  DCHECK(profile_customization_preloaded_contents_);
  dialog_delegate_->SetWebContents(
      profile_customization_preloaded_contents_.get());
  dialog_delegate_->ResizeNativeView(ProfileCustomizationUI::kPreferredHeight);
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
                         ProfileCustomizationCloseOnCompletion,
                     // Unretained is fine because `this` owns the web contents.
                     base::Unretained(this)));
}

void SigninInterceptFirstRunExperienceDialog::OnSyncedThemeReady(
    ProfileCustomizationSyncedThemeWaiter::Outcome outcome) {
  synced_theme_waiter_.reset();
  Step next_step;
  switch (outcome) {
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncSuccess:
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncCannotStart:
      next_step = Step::kProfileCustomization;
      break;
    case ProfileCustomizationSyncedThemeWaiter::Outcome::
        kSyncPassphraseRequired:
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kTimeout:
      next_step = Step::kProfileSwitchIPHAndCloseModal;
      break;
  }
  DoNextStep(Step::kWaitForSyncedTheme, next_step);
}

void SigninInterceptFirstRunExperienceDialog::
    ProfileCustomizationCloseOnCompletion(
        ProfileCustomizationHandler::CustomizationResult customization_result) {
  switch (customization_result) {
    case ProfileCustomizationHandler::CustomizationResult::kDone:
      RecordDialogEvent(DialogEvent::kProfileCustomizationClickDone);
      break;
    case ProfileCustomizationHandler::CustomizationResult::kSkip:
      RecordDialogEvent(DialogEvent::kProfileCustomizationClickSkip);
      break;
  }
  DoNextStep(Step::kProfileCustomization, Step::kProfileSwitchIPHAndCloseModal);
}
