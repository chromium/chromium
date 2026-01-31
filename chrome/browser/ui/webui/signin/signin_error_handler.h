// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "content/public/browser/web_ui_message_handler.h"

class SigninErrorHandler : public content::WebUIMessageHandler,
                           public BrowserCollectionObserver {
 public:
  // Constructor of a message handler that handles messages from the
  // sign-in error WebUI.
  explicit SigninErrorHandler(Browser* browser);

  SigninErrorHandler(const SigninErrorHandler&) = delete;
  SigninErrorHandler& operator=(const SigninErrorHandler&) = delete;

  ~SigninErrorHandler() override;

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

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
  virtual void HandleSwitchToExistingProfile(const base::ListValue& args);

  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user acknowledges the signin error.
  virtual void HandleConfirm(const base::ListValue& args);

  // Handles "learnMore" message from the page. No arguments.
  // This message is sent when the user clicks on the "Learn more" link in the
  // signin error dialog, which closes the dialog and takes the user to the
  // Chrome Help page about fixing sync problems.
  virtual void HandleLearnMore(const base::ListValue& args);

  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  virtual void HandleInitializedWithSize(const base::ListValue& args);

  // CloseDialog will eventually destroy this object, so nothing should access
  // its members after this call.
  void CloseDialog();

  // Closes the modal sign-in view dialog.
  //
  // Virtual, so that it can be overridden from unit tests.
  virtual void CloseBrowserModalSigninDialog();

 private:
  // Weak reference to the browser that showed the sign-in error dialog.
  // This is null when this sign-in error dialog is presented from the user
  // manager.
  raw_ptr<Browser> browser_;

  base::FilePath duplicate_profile_path_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_HANDLER_H_
