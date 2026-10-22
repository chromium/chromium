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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_customization_synced_theme_waiter.h"
#include "chrome/browser/ui/toolbar/avatar_toolbar_iph_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
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
  void ShowSignInCelebration(
      base::OnceClosure celebration_finished) override;

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
  CHECK(syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
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
    InterceptHistorySyncOptinHelperDelegate::ShowSignInCelebration(
        base::OnceClosure celebration_finished) {
  // The celebration screen is not available for the sign-in intercept access
  // point.
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

SigninInterceptFirstRunExperienceDialog::
    SigninInterceptFirstRunExperienceDialog(BrowserWindowInterface* browser,
                                            const CoreAccountId& account_id,
                                            bool is_forced_intercept,
                                            base::OnceClosure on_close_callback)
    : SigninModalDialog(std::move(on_close_callback)),
      browser_(browser),
      account_id_(account_id),
      is_forced_intercept_(is_forced_intercept) {
  CHECK(syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
  // This is a brand new profile. There should be no enterprise confirmation
  // screens offering the option to create another profile. This class handles
  // management.
  auto_accept_management_ =
      enterprise_util::EnabledAutomaticManagementDisclaimerAcceptanceUntilReset(
          browser_->GetProfile());

  if (!base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer)) {
    // Trigger the disclaimer service to auto-approve the management and fetch
    // the applicable policies.
    auto* profile_management_disclaimer_service =
        ProfileManagementDisclaimerServiceFactory::GetForProfile(
            browser_->GetProfile());
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

void SigninInterceptFirstRunExperienceDialog::Show() {
  RecordDialogEvent(DialogEvent::kStart);
  Step next_step = base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)
                       ? Step::kStartHistorySyncOptin
                       : Step::kWaitForSyncedTheme;
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
  CHECK(dialog_delegate_, base::NotFatalUntil::M162);
  dialog_delegate_->ResizeNativeView(height);
}

content::WebContents*
SigninInterceptFirstRunExperienceDialog::GetModalDialogWebContentsForTesting() {
  return dialog_delegate_ ? dialog_delegate_->GetWebContents() : nullptr;
}

void SigninInterceptFirstRunExperienceDialog::OnModalDialogClosed() {
  CHECK(!dialog_delegate_ ||
            dialog_delegate_observation_.IsObservingSource(dialog_delegate_),
        base::NotFatalUntil::M162);
  dialog_delegate_observation_.Reset();
  dialog_delegate_ = nullptr;
  NotifyModalDialogClosed();
}

void SigninInterceptFirstRunExperienceDialog::DoNextStep(
    Step expected_current_step,
    Step step) {
  CHECK_EQ(expected_current_step, current_step_, base::NotFatalUntil::M162);
  // Going to a previous step is not allowed.
  CHECK_GT(step, current_step_, base::NotFatalUntil::M162);
  current_step_ = step;

  switch (step) {
    case Step::kStart:
      NOTREACHED();
    case Step::kStartHistorySyncOptin:
      CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
      CHECK(syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
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

void SigninInterceptFirstRunExperienceDialog::DoStartHistorySync() {
  CHECK(syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->GetProfile());
  CHECK(identity_manager);
  auto extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id_);
  auto history_sync_optin_delegate =
      std::make_unique<InterceptHistorySyncOptinHelperDelegate>(
          weak_ptr_factory_.GetWeakPtr());
  history_sync_optin_delegate_ = history_sync_optin_delegate->GetWeakPtr();
  auto* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(browser_->GetProfile());
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
          SyncServiceFactory::GetForProfile(browser_->GetProfile()),
          ThemeServiceFactory::GetForProfile(browser_->GetProfile()),
          base::BindOnce(
              &SigninInterceptFirstRunExperienceDialog::OnSyncedThemeReady,
              // Unretained() is fine because `this` owns `synced_theme_waiter_`
              base::Unretained(this)));
  synced_theme_waiter_->Run();
}

void SigninInterceptFirstRunExperienceDialog::DoProfileCustomization() {
  // Don't show the customization bubble if a valid policy theme is set.
  if (ThemeServiceFactory::GetForProfile(browser_->GetProfile())
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

  CHECK(profile_customization_preloaded_contents_, base::NotFatalUntil::M162);
  dialog_delegate_->SetWebContents(
      profile_customization_preloaded_contents_.get());
  dialog_delegate_->ResizeNativeView(ProfileCustomizationUI::kPreferredHeight);
}

void SigninInterceptFirstRunExperienceDialog::
    DoProfileSwitchIPHAndCloseModal() {
  AvatarToolbarIphController::From(browser_)->MaybeShowProfileSwitchIPH();
  CloseModalDialog();
}

void SigninInterceptFirstRunExperienceDialog::SetDialogDelegate(
    SigninViewControllerDelegate* delegate) {
  CHECK(!dialog_delegate_, base::NotFatalUntil::M162);
  CHECK(!dialog_delegate_observation_.IsObserving(), base::NotFatalUntil::M162);
  dialog_delegate_ = delegate;
  dialog_delegate_observation_.Observe(dialog_delegate_);
}

void SigninInterceptFirstRunExperienceDialog::PreloadProfileCustomizationUI() {
  profile_customization_preloaded_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          browser_->GetProfile(),
          content::SiteInstance::Create(browser_->GetProfile())));
  profile_customization_preloaded_contents_->GetController().LoadURL(
      GURL(chrome::kChromeUIProfileCustomizationURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  ProfileCustomizationUI* web_ui =
      profile_customization_preloaded_contents_->GetWebUI()
          ->GetController()
          ->GetAs<ProfileCustomizationUI>();
  CHECK(web_ui, base::NotFatalUntil::M162);
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
