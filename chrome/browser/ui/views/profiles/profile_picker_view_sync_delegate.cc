// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"

#include "base/logging.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/webui_url_constants.h"

namespace {

void OpenSettingsInBrowser(Browser* browser) {
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
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
  // TODO(crbug.com/1126913): Handle the error cases.
  NOTIMPLEMENTED();
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
  // TODO(crbug.com/1126913): Handle the error cases.
  NOTIMPLEMENTED();
}

void ProfilePickerViewSyncDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);
  scoped_login_ui_service_observer_.Add(
      LoginUIServiceFactory::GetForProfile(profile_));
  ProfilePicker::SwitchToSyncConfirmation();
}

void ProfilePickerViewSyncDelegate::ShowSyncSettings() {
  // Open the browser and when it's done, open settings in the browser.
  std::move(open_browser_callback_)
      .Run(profile_, base::BindOnce(&OpenSettingsInBrowser));
}

void ProfilePickerViewSyncDelegate::SwitchToProfile(Profile* new_profile) {
  // TODO(crbug.com/1126913): Handle this flow.
  NOTIMPLEMENTED();
}

void ProfilePickerViewSyncDelegate::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // No need to listen to further confirmations any more.
  scoped_login_ui_service_observer_.Remove(
      LoginUIServiceFactory::GetForProfile(profile_));

  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
}
