// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"

#include "base/logging.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/webui_url_constants.h"

namespace {

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

void ProfilePickerViewSyncDelegate::ShowLoginError(
    const std::string& email,
    const std::string& error_message) {
  // Open the browser and when it's done, show the login error.
  // TODO(crbug.com/1126913): In some cases, the current behavior is not ideal
  // because it is not designed with profile creation in mind. Concretely, for
  // sync not being available because there already is a syncing profile with
  // this account, we should likely auto-delete the profile and offer to either
  // switch or to start sign-in once again.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(
               &DiceTurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser, email,
               error_message),
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
  // Open the browser and when it's done, show the confirmation dialog.
  std::move(open_browser_callback_)
      .Run(base::BindOnce(&DiceTurnSyncOnHelper::Delegate::
                              ShowEnterpriseAccountConfirmationForBrowser,
                          email, std::move(callback)),
           /*enterprise_sync_consent_needed=*/true);
}

void ProfilePickerViewSyncDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
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
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

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
