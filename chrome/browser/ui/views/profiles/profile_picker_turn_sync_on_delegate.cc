// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"

#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

absl::optional<ProfileMetrics::ProfileSignedInFlowOutcome> GetSyncOutcome(
    bool enterprise_account,
    bool sync_disabled,
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // The decision of the user is not relevant for the metric.
  if (sync_disabled)
    return ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSyncDisabled;

  switch (result) {
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      return enterprise_account
                 ? ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSync
                 : ProfileMetrics::ProfileSignedInFlowOutcome::kConsumerSync;
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      return enterprise_account ? ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kEnterpriseSyncSettings
                                : ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kConsumerSyncSettings;
    case LoginUIService::ABORT_SYNC:
      return enterprise_account ? ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kEnterpriseSigninOnly
                                : ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kConsumerSigninOnly;
    case LoginUIService::UI_CLOSED:
      // The metric is recorded elsewhere.
      return absl::nullopt;
  }
}

void OpenSettingsInBrowser(Browser* browser) {
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}

bool IsLacrosPrimaryProfileFirstRun(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(profile);
  // The primary profile can never get _created_ through profile creation flow
  // so if it's the primary (main) profile, it must be onboarding.
  return profile->IsMainProfile();
#else
  return false;
#endif
}

}  // namespace

ProfilePickerTurnSyncOnDelegate::ProfilePickerTurnSyncOnDelegate(
    base::WeakPtr<ProfilePickerSignedInFlowController> controller,
    Profile* profile)
    : controller_(controller), profile_(profile) {}

ProfilePickerTurnSyncOnDelegate::~ProfilePickerTurnSyncOnDelegate() = default;

void ProfilePickerTurnSyncOnDelegate::ShowLoginError(
    const SigninUIError& error) {
  LogOutcome(ProfileMetrics::ProfileSignedInFlowOutcome::kLoginError);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | Login error: "
               << static_cast<int>(error.type());
#endif
  if (IsLacrosPrimaryProfileFirstRun(profile_)) {
    // The primary profile onboarding is silently skipped if there's any error.
    if (controller_)
      controller_->FinishAndOpenBrowser(PostHostClearedCallback());
    return;
  }

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
    controller_->FinishAndOpenBrowser(PostHostClearedCallback(base::BindOnce(
        &TurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser, error)));
  }
}

void ProfilePickerTurnSyncOnDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    signin::SigninChoiceCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | Unexpected data merge prompt";
#endif
  // A brand new profile cannot have a conflict in sync accounts.
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback) {
  enterprise_account_ = true;
  // In this flow, the enterprise confirmation is replaced by an enterprise
  // welcome screen. Knowing if sync is enabled is needed for the screen. Thus,
  // it is delayed until either ShowSyncConfirmation() or
  // ShowSyncDisabledConfirmation() gets called.
  // Assume an implicit "Continue" here.
  std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  return;
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);

  absl::optional<EnterpriseProfileWelcomeUI::ScreenType> welcome_screen_type;
  if (IsLacrosPrimaryProfileFirstRun(profile_)) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Show the enterprise version of the screen even if management consent was
    // already given. See http://crbug.com/1322067.
    enterprise_account_ = profile_->GetProfilePolicyConnector()->IsManaged();

    welcome_screen_type =
        enterprise_account_
            ? EnterpriseProfileWelcomeUI::ScreenType::kLacrosEnterpriseWelcome
            : EnterpriseProfileWelcomeUI::ScreenType::kLacrosConsumerWelcome;
#endif
  } else if (enterprise_account_) {
    welcome_screen_type =
        EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled;
  }

  if (welcome_screen_type) {
    // First show the welcome screen and only after that (if the user proceeds
    // with the flow) the sync consent.
    ShowEnterpriseWelcome(*welcome_screen_type);
    return;
  }

  ShowSyncConfirmationScreen();
}

bool ProfilePickerTurnSyncOnDelegate::
    ShouldAbortBeforeShowSyncDisabledConfirmation() {
  if (IsLacrosPrimaryProfileFirstRun(profile_)) {
    // The primary profile first run experience is silently skipped if sync is
    // disabled (there's no point to promo a feature that cannot get enabled).
    return true;
  }

  return false;
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | Sync disabled prompt.";
#endif
  DCHECK(callback);
  DCHECK(!IsLacrosPrimaryProfileFirstRun(profile_));
  sync_disabled_ = true;

  sync_confirmation_callback_ = std::move(callback);
  ShowEnterpriseWelcome(is_managed_account
                            ? EnterpriseProfileWelcomeUI::ScreenType::
                                  kEntepriseAccountSyncDisabled
                            : EnterpriseProfileWelcomeUI::ScreenType::
                                  kConsumerAccountSyncDisabled);
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncSettings() {
  // Open the browser and when it's done, open settings in the browser.
  if (controller_) {
    controller_->FinishAndOpenBrowser(
        PostHostClearedCallback(base::BindOnce(&OpenSettingsInBrowser)));
  }
}

void ProfilePickerTurnSyncOnDelegate::SwitchToProfile(Profile* new_profile) {
  // A brand new profile cannot have preexisting syncable data and thus
  // switching to another profile does never get offered.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | SwitchToProfile not expected.";
#endif
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // No need to listen to further confirmations any more.
  DCHECK(scoped_login_ui_service_observation_.IsObservingSource(
      LoginUIServiceFactory::GetForProfile(profile_)));
  scoped_login_ui_service_observation_.Reset();

  absl::optional<ProfileMetrics::ProfileSignedInFlowOutcome> outcome =
      GetSyncOutcome(enterprise_account_, sync_disabled_, result);
  if (outcome) {
    LogOutcome(*outcome);
  } else if (IsLacrosPrimaryProfileFirstRun(profile_) &&
             result == LoginUIService::UI_CLOSED) {
    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedAfterSignIn);
  }

  FinishSyncConfirmation(result);
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
    LoginUIService::SyncConfirmationUIClosedResult result) {
  DCHECK(sync_confirmation_callback_);
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
    signin::SigninChoice choice) {
  if (choice == signin::SIGNIN_CHOICE_CANCEL) {
    LogOutcome(ProfileMetrics::ProfileSignedInFlowOutcome::
                   kAbortedOnEnterpriseWelcome);
    // The callback provided by TurnSyncOnHelper must be called, UI_CLOSED
    // makes sure the final callback does not get called. It does not matter
    // what happens to sync as the signed-in profile creation gets cancelled
    // right after.
    FinishSyncConfirmation(LoginUIService::UI_CLOSED);
    ProfilePicker::CancelSignedInFlow();
    return;
  }

  DCHECK_EQ(choice, signin::SIGNIN_CHOICE_NEW_PROFILE);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | Closed EnterpriseWelcome with choice="
               << static_cast<int>(choice)
               << " and type=" << static_cast<int>(type);
#endif

  switch (type) {
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    case EnterpriseProfileWelcomeUI::ScreenType::kLacrosConsumerWelcome:
    case EnterpriseProfileWelcomeUI::ScreenType::kLacrosEnterpriseWelcome:
#endif
      ShowSyncConfirmationScreen();
      return;
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncDisabled:
    case EnterpriseProfileWelcomeUI::ScreenType::kConsumerAccountSyncDisabled:
      // Logging kEnterpriseSyncDisabled for consumer accounts on managed
      // devices is a pre-existing minor imprecision in reporting of this metric
      // that's not worth fixing.
      LogOutcome(
          ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSyncDisabled);
      // SYNC_WITH_DEFAULT_SETTINGS encodes that the user wants to continue
      // (despite sync being disabled).
      // TODO (crbug.com/1141341): Split the enum for sync disabled / rename the
      // entries to better match the situation.
      FinishSyncConfirmation(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
      break;
    case EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation:
      NOTREACHED() << "The profile picker should not show an enterprise "
                      "welcome that prompts for profile creation";
  }
}

void ProfilePickerTurnSyncOnDelegate::LogOutcome(
    ProfileMetrics::ProfileSignedInFlowOutcome outcome) {
  if (IsLacrosPrimaryProfileFirstRun(profile_)) {
    ProfileMetrics::LogLacrosPrimaryProfileFirstRunOutcome(outcome);
  } else {
    ProfileMetrics::LogProfileAddSignInFlowOutcome(outcome);
  }
}
