// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
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

void RecordSyncOutcomeAndRunCallback(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback,
    bool enterprise_confirmation_shown,
    LoginUIService::SyncConfirmationUIClosedResult result) {
  switch (result) {
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          enterprise_confirmation_shown
              ? ProfileMetrics::ProfileAddSignInFlowOutcome::kEnterpriseSync
              : ProfileMetrics::ProfileAddSignInFlowOutcome::kConsumerSync);
      break;
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          enterprise_confirmation_shown
              ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                    kEnterpriseSyncSettings
              : ProfileMetrics::ProfileAddSignInFlowOutcome::
                    kConsumerSyncSettings);
      break;
    case LoginUIService::ABORT_SYNC:
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          enterprise_confirmation_shown
              ? ProfileMetrics::ProfileAddSignInFlowOutcome::
                    kEnterpriseSigninOnly
              : ProfileMetrics::ProfileAddSignInFlowOutcome::
                    kConsumerSigninOnly);
      break;
    case LoginUIService::UI_CLOSED:
      // The metric is recorded elsewhere.
      break;
  }
  std::move(callback).Run(result);
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

  // Open the browser and when it's done, show the login error.
  // TODO(crbug.com/1126913): In some cases, the current behavior is not ideal
  // because it is not designed with profile creation in mind. Concretely, for
  // sync not being available because there already is a syncing profile with
  // this account, we should likely auto-delete the profile and offer to either
  // switch or to start sign-in once again.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(
               &DiceTurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser,
               error.email(), error.message()),
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
  enterprise_confirmation_shown_ = true;
  // If the user rejects the confirmation, record the outcome.
  DiceTurnSyncOnHelper::SigninChoiceCallback wrapped_callback = base::BindOnce(
      &MaybeRecordEnterpriseRejectionAndRunCallback, std::move(callback));
  // Open the browser and when it's done, show the confirmation dialog.
  // We have a guarantee that the profile is brand new, no need to prompt for
  // another profile.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(&DiceTurnSyncOnHelper::Delegate::
                              ShowEnterpriseAccountConfirmationForBrowser,
                          email, /*prompt_for_new_profile=*/false,
                          std::move(wrapped_callback)),
           /*enterprise_sync_consent_needed=*/true);
}

void ProfilePickerViewSyncDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  // Record the outcome once the user responds.
  sync_confirmation_callback_ =
      base::BindOnce(&RecordSyncOutcomeAndRunCallback, std::move(callback),
                     enterprise_confirmation_shown_);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

  if (enterprise_confirmation_shown_) {
    OpenSyncConfirmationDialogInBrowser(
        chrome::FindLastActiveWithProfile(profile_));
    return;
  }

  ProfilePicker::SwitchToSyncConfirmation();
}

void ProfilePickerViewSyncDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

  // Record the outcome, the decision of the user is not relevant for the
  // metric.
  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kEnterpriseSyncDisabled);

  // Enterprise confirmation may or may not be shown before showing the disabled
  // confirmation (in both cases it is disabled by an enterprise).
  if (enterprise_confirmation_shown_) {
    OpenSyncConfirmationDialogInBrowser(
        chrome::FindLastActiveWithProfile(profile_));
    return;
  }

  // Open the browser and when it's done, show the confirmation dialog.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(&OpenSyncConfirmationDialogInBrowser),
           /*enterprise_sync_consent_needed=*/false);
}

void ProfilePickerViewSyncDelegate::ShowSyncSettings() {
  if (enterprise_confirmation_shown_) {
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

  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
}
