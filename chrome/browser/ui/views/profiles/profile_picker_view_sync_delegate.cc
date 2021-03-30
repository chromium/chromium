// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"

#include "base/logging.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/webui_url_constants.h"

namespace {

void MaybeRecordEnterpriseRejectionAndRunCallback(
    DiceTurnSyncOnHelper::SigninChoiceCallback callback,
    DiceTurnSyncOnHelper::SigninChoice choice) {
  if (choice == DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL) {
    // If the user decides to not link the profile, the flow stops here.
    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileAddSignInFlowOutcome::
            kEnterpriseSigninOnlyNotLinked);
  }
  std::move(callback).Run(choice);
}

base::Optional<ProfileMetrics::ProfileAddSignInFlowOutcome> GetSyncOutcome(
    bool enterprise_account,
    bool sync_disabled,
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // The decision of the user is not relevant for the metric.
  if (sync_disabled)
    return ProfileMetrics::ProfileAddSignInFlowOutcome::kEnterpriseSyncDisabled;

  switch (result) {
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      return enterprise_account
                 ? ProfileMetrics::ProfileAddSignInFlowOutcome::kEnterpriseSync
                 : ProfileMetrics::ProfileAddSignInFlowOutcome::kConsumerSync;
      break;
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      return enterprise_account ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kEnterpriseSyncSettings
                                : ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kConsumerSyncSettings;
      break;
    case LoginUIService::ABORT_SYNC:
      return enterprise_account ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kEnterpriseSigninOnly
                                : ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kConsumerSigninOnly;
      break;
    case LoginUIService::UI_CLOSED:
      // The metric is recorded elsewhere.
      return base::nullopt;
  }
}

void OpenSettingsInBrowser(Browser* browser) {
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}

void OpenSyncConfirmationDialogInBrowser(Browser* browser) {
  // This is a very rare corner case (e.g. the user manages to close the only
  // browser window in a very short span of time between enterprise
  // confirmation and this callback), not worth handling fully. Instead, the
  // flow is aborted.
  if (!browser)
    return;
  browser->signin_view_controller()->ShowModalSyncConfirmationDialog();
}

bool IsEnterpriseFlowEnabled() {
  return base::FeatureList::IsEnabled(
      features::kSignInProfileCreationEnterprise);
}

}  // namespace

ProfilePickerViewSyncDelegate::ProfilePickerViewSyncDelegate(
    Profile* profile,
    OpenBrowserCallback open_browser_callback)
    : profile_(profile),
      open_browser_callback_(std::move(open_browser_callback)) {}

ProfilePickerViewSyncDelegate::~ProfilePickerViewSyncDelegate() = default;

void ProfilePickerViewSyncDelegate::ShowLoginError(const SigninUIError& error) {
  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kLoginError);

  // Show the profile switch confirmation screen inside of the profile picker if
  // the user cannot sign in because the account already used by another
  // profile.
  if (error.type() ==
          SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile &&
      IsEnterpriseFlowEnabled()) {
    ProfilePicker::SwitchToProfileSwitch(error.another_profile_path());
    return;
  }

  // Open the browser and when it's done, show the login error.
  // TODO(crbug.com/1126913): In some cases, the current behavior is not ideal
  // because it is not designed with profile creation in mind. Concretely, for
  // sync not being available because there already is a syncing profile with
  // this account, we should likely auto-delete the profile and offer to either
  // switch or to start sign-in once again.
  std::move(open_browser_callback_)
      .Run(
          base::BindOnce(
              &DiceTurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser, error),
          /*enterprise_sync_consent_needed=*/false);
}

void ProfilePickerViewSyncDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  // A brand new profile cannot have a conflict in sync accounts.
  NOTREACHED();
}

void ProfilePickerViewSyncDelegate::ShowEnterpriseAccountConfirmation(
    const std::string& email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  enterprise_account_ = true;
  if (!IsEnterpriseFlowEnabled()) {
    // If the user rejects the confirmation, record the outcome.
    DiceTurnSyncOnHelper::SigninChoiceCallback wrapped_callback =
        base::BindOnce(&MaybeRecordEnterpriseRejectionAndRunCallback,
                       std::move(callback));
    // Open the browser and when it's done, show the confirmation dialog.
    // We have a guarantee that the profile is brand new, no need to prompt for
    // another profile.
    std::move(open_browser_callback_)
        .Run(base::BindOnce(&DiceTurnSyncOnHelper::Delegate::
                                ShowEnterpriseAccountConfirmationForBrowser,
                            email, /*prompt_for_new_profile=*/false,
                            std::move(wrapped_callback)),
             /*enterprise_sync_consent_needed=*/true);
    return;
  }

  // In this flow, the enterprise confirmation is replaced by an enterprise
  // welcome screen. Knowing if sync is enabled is needed for the screen. Thus,
  // it is delayed until either ShowSyncConfirmation() or
  // ShowSyncDisabledConfirmation() gets called.
  // Assume an implicit "Continue" here.
  std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  return;
}

void ProfilePickerViewSyncDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);

  if (enterprise_account_) {
    if (!IsEnterpriseFlowEnabled()) {
      DCHECK(!scoped_login_ui_service_observation_.IsObserving());
      scoped_login_ui_service_observation_.Observe(
          LoginUIServiceFactory::GetForProfile(profile_));
      OpenSyncConfirmationDialogInBrowser(
          chrome::FindLastActiveWithProfile(profile_));
      return;
    }

    // First show the enterprise welcome screen and only after that (if the user
    // proceeds with the flow) the sync consent.
    ShowEnterpriseWelcome(
        EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled);
    return;
  }

  ShowSyncConfirmationScreen();
}

void ProfilePickerViewSyncDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  sync_disabled_ = true;

  if (!IsEnterpriseFlowEnabled()) {
    DCHECK(!scoped_login_ui_service_observation_.IsObserving());
    scoped_login_ui_service_observation_.Observe(
        LoginUIServiceFactory::GetForProfile(profile_));

    // Enterprise confirmation may or may not be shown before showing the
    // disabled confirmation (in both cases it is disabled by an enterprise).
    if (enterprise_account_) {
      OpenSyncConfirmationDialogInBrowser(
          chrome::FindLastActiveWithProfile(profile_));
      return;
    }

    // Open the browser and when it's done, show the confirmation dialog.
    std::move(open_browser_callback_)
        .Run(base::BindOnce(&OpenSyncConfirmationDialogInBrowser),
             /*enterprise_sync_consent_needed=*/false);
    return;
  }

  ShowEnterpriseWelcome(is_managed_account
                            ? EnterpriseProfileWelcomeUI::ScreenType::
                                  kEntepriseAccountSyncDisabled
                            : EnterpriseProfileWelcomeUI::ScreenType::
                                  kConsumerAccountSyncDisabled);
}

void ProfilePickerViewSyncDelegate::ShowSyncSettings() {
  if (enterprise_account_ && !IsEnterpriseFlowEnabled()) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile_);
    if (!browser)
      return;
    OpenSettingsInBrowser(browser);
    return;
  }

  // Open the browser and when it's done, open settings in the browser.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(&OpenSettingsInBrowser),
           /*enterprise_sync_consent_needed=*/false);
}

void ProfilePickerViewSyncDelegate::SwitchToProfile(Profile* new_profile) {
  // A brand new profile cannot have preexisting syncable data and thus
  // switching to another profile does never get offered.
  NOTREACHED();
}

void ProfilePickerViewSyncDelegate::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // No need to listen to further confirmations any more.
  DCHECK(scoped_login_ui_service_observation_.IsObservingSource(
      LoginUIServiceFactory::GetForProfile(profile_)));
  scoped_login_ui_service_observation_.Reset();

  FinishSyncConfirmation(
      result, GetSyncOutcome(enterprise_account_, sync_disabled_, result));
}

void ProfilePickerViewSyncDelegate::ShowSyncConfirmationScreen() {
  DCHECK(sync_confirmation_callback_);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

  ProfilePicker::SwitchToSyncConfirmation();
}

void ProfilePickerViewSyncDelegate::FinishSyncConfirmation(
    LoginUIService::SyncConfirmationUIClosedResult result,
    base::Optional<ProfileMetrics::ProfileAddSignInFlowOutcome> outcome) {
  DCHECK(sync_confirmation_callback_);
  if (outcome)
    ProfileMetrics::LogProfileAddSignInFlowOutcome(*outcome);
  std::move(sync_confirmation_callback_).Run(result);
}

void ProfilePickerViewSyncDelegate::ShowEnterpriseWelcome(
    EnterpriseProfileWelcomeUI::ScreenType type) {
  DCHECK(sync_confirmation_callback_);
  // Unretained as the delegate lives until `sync_confirmation_callback_` gets
  // called and thus always outlives the enterprise screen.
  ProfilePicker::SwitchToEnterpriseProfileWelcome(
      type,
      base::BindOnce(&ProfilePickerViewSyncDelegate::OnEnterpriseWelcomeClosed,
                     base::Unretained(this), type));
}

void ProfilePickerViewSyncDelegate::OnEnterpriseWelcomeClosed(
    EnterpriseProfileWelcomeUI::ScreenType type,
    bool proceed) {
  if (!proceed) {
    // The callback provided by DiceTurnSyncOnHelper must be called, UI_CLOSED
    // makes sure the final callback does not get called. It does not matter
    // what happens to sync as the signed-in profile creation gets cancelled
    // right after.
    FinishSyncConfirmation(LoginUIService::UI_CLOSED,
                           ProfileMetrics::ProfileAddSignInFlowOutcome::
                               kAbortedOnEnterpriseWelcome);
    ProfilePicker::CancelSignIn();
    return;
  }

  switch (type) {
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled:
      ShowSyncConfirmationScreen();
      return;
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncDisabled:
    case EnterpriseProfileWelcomeUI::ScreenType::kConsumerAccountSyncDisabled:
      // SYNC_WITH_DEFAULT_SETTINGS encodes that the user wants to continue
      // (despite sync being disabled).
      // TODO (crbug.com/1141341): Split the enum for sync disabled / rename the
      // entries to better match the situation.
      // Logging kEnterpriseSyncDisabled for consumer accounts on managed
      // devices is a pre-existing minor imprecision in reporting of this metric
      // that's not worth fixing.
      FinishSyncConfirmation(
          LoginUIService::SYNC_WITH_DEFAULT_SETTINGS,
          ProfileMetrics::ProfileAddSignInFlowOutcome::kEnterpriseSyncDisabled);
  }
}
