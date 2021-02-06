// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

class SigninErrorHandler : public content::WebUIMessageHandler,
                           public BrowserListObserver {
 public:
  // Constructor of a message handler that handles messages from the
  // sign-in error WebUI.
  // If |is_system_profile| is true, then the sign-in error dialog was
  // presented from the user manager and |browser| is null. Otherwise, the
  // sign-in error dialog was presented on a browser window and |browser| must
  // not be null.
  SigninErrorHandler(Browser* browser, bool is_system_profile);
  ~SigninErrorHandler() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Sets the existing profile path that has the same username used for signin.
  // This function is called when the signin error is a duplicate account error.
  void set_duplicate_profile_path(
      const base::FilePath& duplicate_profile_path) {
    duplicate_profile_path_ = duplicate_profile_path;
  }

 protected:
  // Handles "switch" message from the page. No arguments.
  // This message is sent when the user switches to the existing profile of the
  // same username used for signin.
  virtual void HandleSwitchToExistingProfile(const base::ListValue* args);

  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user acknowledges the signin error.
  virtual void HandleConfirm(const base::ListValue* args);

  // Handles "learnMore" message from the page. No arguments.
  // This message is sent when the user clicks on the "Learn more" link in the
  // signin error dialog, which closes the dialog and takes the user to the
  // Chrome Help page about fixing sync problems.
  virtual void HandleLearnMore(const base::ListValue* args);

  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  virtual void HandleInitializedWithSize(const base::ListValue* args);

  // CloseDialog will eventually destroy this object, so nothing should access
  // its members after this call.
  void CloseDialog();

  // Closes the modal sign-in view dialog.
  //
  // Virtual, so that it can be overriden from unit tests.
  virtual void CloseBrowserModalSigninDialog();

  // Closes the user manager profile dialog.
  //
  // Virtual, so that it can be overriden from unit tests.
  virtual void CloseUserManagerProfileDialog();

 private:
  // Weak reference to the browser that showed the sign-in error dialog.
  // This is null when this sign-in error dialog is presented from the user
  // manager.
  Browser* browser_;

  // True when this sign-in error dialog is presented from the user manager.
  bool is_system_profile_;

  base::FilePath duplicate_profile_path_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_
