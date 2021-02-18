// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

SigninErrorHandler::SigninErrorHandler(Browser* browser, bool is_system_profile)
    : browser_(browser), is_system_profile_(is_system_profile) {
  // |browser_| must not be null when this dialog is presented from the
  // profile picker.
  DCHECK(browser_ || is_system_profile_);
  BrowserList::AddObserver(this);
}

SigninErrorHandler::~SigninErrorHandler() {
  BrowserList::RemoveObserver(this);
}

void SigninErrorHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void SigninErrorHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SigninErrorHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "switchToExistingProfile",
      base::BindRepeating(&SigninErrorHandler::HandleSwitchToExistingProfile,
                          base::Unretained(this)));
  if (!is_system_profile_) {
    web_ui()->RegisterMessageCallback(
        "learnMore", base::BindRepeating(&SigninErrorHandler::HandleLearnMore,
                                         base::Unretained(this)));
  }
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(&SigninErrorHandler::HandleInitializedWithSize,
                          base::Unretained(this)));
}

void SigninErrorHandler::HandleSwitchToExistingProfile(
    const base::ListValue* args) {
  if (duplicate_profile_path_.empty())
    return;

  // CloseDialog will eventually destroy this object, so nothing should access
  // its members after this call. However, closing the dialog may steal focus
  // back to the original window, so make a copy of the path to switch to and
  // perform the switch after the dialog is closed.
  base::FilePath path_switching_to = duplicate_profile_path_;
  CloseDialog();

  // Switch to the existing duplicate profile. Do not create a new window when
  // any existing ones can be reused.
  profiles::SwitchToProfile(path_switching_to, false,
                            ProfileManager::CreateCallback());
}

void SigninErrorHandler::HandleConfirm(const base::ListValue* args) {
  CloseDialog();
}

void SigninErrorHandler::HandleLearnMore(const base::ListValue* args) {
  // "Learn more" only shown when is_system_profile_=false
  DCHECK(!is_system_profile_);
  if (!browser_)
    return;
  CloseDialog();
  signin_ui_util::ShowSigninErrorLearnMorePage(browser_->profile());
}

void SigninErrorHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  AllowJavascript();
  if (duplicate_profile_path_.empty())
    FireWebUIListener("switch-button-unavailable");

  signin::SetInitializedModalHeight(browser_, web_ui(), args);
}

void SigninErrorHandler::CloseDialog() {
  if (is_system_profile_) {
    CloseProfilePickerForceSigninDialog();
  } else if (browser_){
    CloseBrowserModalSigninDialog();
  }
}

void SigninErrorHandler::CloseBrowserModalSigninDialog() {
  browser_->signin_view_controller()->CloseModalSignin();
}

void SigninErrorHandler::CloseProfilePickerForceSigninDialog() {
  ProfilePickerForceSigninDialog::HideDialog();
}
