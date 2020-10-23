// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class Profile;

// The LoginUIService helps track per-profile information for the login related
// UIs - for example, whether there is login UI currently on-screen.
class LoginUIService : public KeyedService {
 public:
  // Various UI components implement this API to allow LoginUIService to
  // manipulate their associated login UI.
  class LoginUI {
   public:
    // Invoked when the login UI should be brought to the foreground.
    virtual void FocusUI() = 0;

   protected:
    virtual ~LoginUI() {}
  };

  // Used when the sync confirmation UI is closed to signify which option was
  // selected by the user.
  enum SyncConfirmationUIClosedResult {
    // Start sync immediately.
    SYNC_WITH_DEFAULT_SETTINGS,
    // Show the user the sync settings before starting sync.
    CONFIGURE_SYNC_FIRST,
    // The signing process was aborted, don't start sync or show settings.
    ABORT_SIGNIN,
  };

  // Interface for obervers of LoginUIService.
  class Observer {
   public:
    // Called when the sync confirmation UI is closed. |result| indicates the
    // option chosen by the user in the confirmation UI.
    virtual void OnSyncConfirmationUIClosed(
        SyncConfirmationUIClosedResult result) {}

   protected:
    virtual ~Observer() {}
  };

  explicit LoginUIService(Profile* profile);
  ~LoginUIService() override;

  // |observer| The observer to add or remove; cannot be NULL.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the currently active login UI, or null if no login UI is active.
  LoginUI* current_login_ui() const;

  // Sets the currently active login UI. Callers must call LoginUIClosed when
  // |ui| is no longer valid.
  void SetLoginUI(LoginUI* ui);

  // Called when login UI is closed.
  void LoginUIClosed(LoginUI* ui);

  // Called when the sync confirmation UI is closed. |result| indicates the
  // option chosen by the user in the confirmation UI.
  void SyncConfirmationUIClosed(SyncConfirmationUIClosedResult result);

  // Delegate to an existing login tab if one exists. If not, a new sigin tab is
  // created.
  void ShowExtensionLoginPrompt(bool enable_sync,
                                const std::string& email_hint);

  // Displays login results. This is either the Modal Signin Error dialog if
  // |error_message| is a non-empty string, or the User Menu with a blue header
  // toast otherwise.
  virtual void DisplayLoginResult(Browser* browser,
                                  const base::string16& error_message,
                                  const base::string16& email);

  // Set the profile blocking modal error dialog message.
  virtual void SetProfileBlockingErrorMessage();

  // Gets whether the Modal Signin Error dialog should display profile blocking
  // error message.
  bool IsDisplayingProfileBlockedErrorMessage() const;

  // Gets the last login result set through |DisplayLoginResult|.
  const base::string16& GetLastLoginResult() const;

  // Gets the last email used for signing in when a signin error occured; set
  // through |DisplayLoginResult|.
  const base::string16& GetLastLoginErrorEmail() const;

 private:
  // Weak pointers to the recently opened UIs, with the most recent in front.
  std::list<LoginUI*> ui_list_;
#if !defined(OS_CHROMEOS)
  Profile* profile_;
#endif

  // List of observers.
  base::ObserverList<Observer>::Unchecked observer_list_;

  base::string16 last_login_result_;
  base::string16 last_login_error_email_;
  bool is_displaying_profile_blocking_error_message_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginUIService);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
