// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"

#include "base/logging.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/webui_url_constants.h"

namespace {

absl::optional<ProfileMetrics::ProfileAddSignInFlowOutcome> GetSyncOutcome(
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
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      return enterprise_account ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kEnterpriseSyncSettings
                                : ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kConsumerSyncSettings;
    case LoginUIService::ABORT_SYNC:
      return enterprise_account ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kEnterpriseSigninOnly
                                : ProfileMetrics::ProfileAddSignInFlowOutcome::
                                      kConsumerSigninOnly;
    case LoginUIService::UI_CLOSED:
      // The metric is recorded elsewhere.
      return absl::nullopt;
  }
}

void OpenSettingsInBrowser(Browser* browser) {
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}

}  // namespace

ProfilePickerTurnSyncOnDelegate::ProfilePickerTurnSyncOnDelegate(
    base::WeakPtr<ProfilePickerSignedInFlowController> controller,
    Profile* profile)
    : controller_(controller), profile_(profile) {}

ProfilePickerTurnSyncOnDelegate::~ProfilePickerTurnSyncOnDelegate() = default;

void ProfilePickerTurnSyncOnDelegate::ShowLoginError(
    const SigninUIError& error) {
  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kLoginError);

  // Show the profile switch confirmation screen inside of the profile picker if
  // the user cannot sign in because the account already used by another
  // profile.
  if (error.type() ==
      SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile) {
    if (controller_) {
      controller_->SwitchToProfileSwitch(error.another_profile_path());
    }
    return;
  }

  // Open the browser and when it's done, show the login error.
  if (controller_) {
    controller_->FinishAndOpenBrowser(base::BindOnce(
        &DiceTurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser, error));
  }
}

void ProfilePickerTurnSyncOnDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  // A brand new profile cannot have a conflict in sync accounts.
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  enterprise_account_ = true;
  // In this flow, the enterprise confirmation is replaced by an enterprise
  // welcome screen. Knowing if sync is enabled is needed for the screen. Thus,
  // it is delayed until either ShowSyncConfirmation() or
  // ShowSyncDisabledConfirmation() gets called.
  // Assume an implicit "Continue" here.
  std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  return;
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);

  if (enterprise_account_) {
    // First show the enterprise welcome screen and only after that (if the user
    // proceeds with the flow) the sync consent.
    ShowEnterpriseWelcome(
        EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled);
    return;
  }

  ShowSyncConfirmationScreen();
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  sync_disabled_ = true;
  ShowEnterpriseWelcome(is_managed_account
                            ? EnterpriseProfileWelcomeUI::ScreenType::
                                  kEntepriseAccountSyncDisabled
                            : EnterpriseProfileWelcomeUI::ScreenType::
                                  kConsumerAccountSyncDisabled);
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncSettings() {
  // Open the browser and when it's done, open settings in the browser.
  if (controller_) {
    controller_->FinishAndOpenBrowser(base::BindOnce(&OpenSettingsInBrowser));
  }
}

void ProfilePickerTurnSyncOnDelegate::SwitchToProfile(Profile* new_profile) {
  // A brand new profile cannot have preexisting syncable data and thus
  // switching to another profile does never get offered.
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // No need to listen to further confirmations any more.
  DCHECK(scoped_login_ui_service_observation_.IsObservingSource(
      LoginUIServiceFactory::GetForProfile(profile_)));
  scoped_login_ui_service_observation_.Reset();

  FinishSyncConfirmation(
      result, GetSyncOutcome(enterprise_account_, sync_disabled_, result));
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncConfirmationScreen() {
  DCHECK(sync_confirmation_callback_);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

  if (controller_)
    controller_->SwitchToSyncConfirmation();
}

void ProfilePickerTurnSyncOnDelegate::FinishSyncConfirmation(
    LoginUIService::SyncConfirmationUIClosedResult result,
    absl::optional<ProfileMetrics::ProfileAddSignInFlowOutcome> outcome) {
  DCHECK(sync_confirmation_callback_);
  if (outcome)
    ProfileMetrics::LogProfileAddSignInFlowOutcome(*outcome);
  std::move(sync_confirmation_callback_).Run(result);
}

void ProfilePickerTurnSyncOnDelegate::ShowEnterpriseWelcome(
    EnterpriseProfileWelcomeUI::ScreenType type) {
  DCHECK(sync_confirmation_callback_);
  // Unretained as the delegate lives until `sync_confirmation_callback_` gets
  // called and thus always outlives the enterprise screen.
  if (controller_) {
    controller_->SwitchToEnterpriseProfileWelcome(
        type, base::BindOnce(
                  &ProfilePickerTurnSyncOnDelegate::OnEnterpriseWelcomeClosed,
                  base::Unretained(this), type));
  }
}

void ProfilePickerTurnSyncOnDelegate::OnEnterpriseWelcomeClosed(
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
    ProfilePicker::CancelSignedInFlow();
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
      break;
    case EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation:
      NOTREACHED() << "The profile picker should not show an enterprise "
                      "welcome that prompts for profile creation";
  }
}
