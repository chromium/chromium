// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
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
    // TODO(crbug.com/40727110): Rename the first option to make it work better
    // for the sync-disabled variant of the UI.
    // Start sync immediately, if sync can be enabled. Otherwise, keep the user
    // signed in (with sync disabled).
    SYNC_WITH_DEFAULT_SETTINGS,
    // Show the user the sync settings before starting sync.
    CONFIGURE_SYNC_FIRST,
    // Turn sync on process was aborted, don't start sync or show settings.
    ABORT_SYNC,
    // The dialog got closed without any explicit user action. The impact of
    // this action depends on the particular flow.
    UI_CLOSED,
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

  LoginUIService(const LoginUIService&) = delete;
  LoginUIService& operator=(const LoginUIService&) = delete;

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

  // If `error.message()` is not empty, displays login error message:
  // - in the Modal Signin Error dialog if `browser` is not null, otherwise
  // - in a dialog shown on top of the profile picker if `from_profile_picker`
  //   is true.
  void DisplayLoginResult(Browser* browser,
                          const SigninUIError& error,
                          bool from_profile_picker);

  // Set the profile blocking modal error dialog message.
  void SetProfileBlockingErrorMessage();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets the last error set through |DisplayLoginResult|.
  const SigninUIError& GetLastLoginError() const;
#endif

 private:
  // Weak pointers to the recently opened UIs, with the most recent in front.
  std::list<raw_ptr<LoginUI, CtnExperimental>> ui_list_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<Profile> profile_;
  SigninUIError last_login_error_ = SigninUIError::Ok();
#endif

  // List of observers.
  base::ObserverList<Observer>::Unchecked observer_list_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
