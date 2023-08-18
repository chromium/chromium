// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

SigninErrorHandler::SigninErrorHandler(Browser* browser,
                                       bool from_profile_picker)
    : browser_(browser), from_profile_picker_(from_profile_picker) {
  // |browser_| must not be null when this dialog is presented from the
  // profile picker.
  DCHECK(browser_ || from_profile_picker_);
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
  if (!from_profile_picker_) {
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
    const base::Value::List& args) {
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
  profiles::SwitchToProfile(path_switching_to, false);
}

void SigninErrorHandler::HandleConfirm(const base::Value::List& args) {
  CloseDialog();
}

void SigninErrorHandler::HandleLearnMore(const base::Value::List& args) {
  // "Learn more" only shown when from_profile_picker_=false
  DCHECK(!from_profile_picker_);
  if (!browser_)
    return;
  CloseDialog();
  signin_ui_util::ShowSigninErrorLearnMorePage(browser_->profile());
}

void SigninErrorHandler::HandleInitializedWithSize(
    const base::Value::List& args) {
  AllowJavascript();
  if (duplicate_profile_path_.empty())
    FireWebUIListener("switch-button-unavailable");

  signin::SetInitializedModalHeight(browser_, web_ui(), args);
}

void SigninErrorHandler::CloseDialog() {
  if (from_profile_picker_) {
    CloseProfilePickerDialog();
  } else if (browser_) {
    CloseBrowserModalSigninDialog();
  }
}

void SigninErrorHandler::CloseBrowserModalSigninDialog() {
  browser_->signin_view_controller()->CloseModalSignin();
}

void SigninErrorHandler::CloseProfilePickerDialog() {
  ProfilePicker::HideDialog();
}
