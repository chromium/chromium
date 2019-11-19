// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// Handler for the 'create profile' page.
class SigninCreateProfileHandler : public content::WebUIMessageHandler,
                                   public BrowserListObserver {
 public:
  SigninCreateProfileHandler();
  ~SigninCreateProfileHandler() override;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 protected:
  // These methods are virtual for testing.
  // Opens a new window for |profile|.
  virtual void OpenNewWindowForProfile(Profile* profile,
                                       Profile::CreateStatus status);

  // Opens a new signin dialog for |profile|.
  virtual void OpenForceSigninDialogForProfile(Profile* profile);

  // Asynchronously creates and initializes a new profile.
  virtual void DoCreateProfile(const base::string16& name,
                               const std::string& icon_url,
                               bool create_shortcut);

 private:
  friend class TestSigninCreateProfileHandler;
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ReturnDefaultProfileIcons);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ReturnSignedInProfiles);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CreateProfile);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CreateProfileWithForceSignin);
  // Represents the final profile creation status. It is used to map
  // the status to the javascript method to be called.
  enum ProfileCreationStatus {
    PROFILE_CREATION_SUCCESS,
    PROFILE_CREATION_ERROR,
  };

  // Represents the type of the in progress profile creation operation.
  // It is used to map the type of the profile creation operation to the
  // correct UMA metric name.
  enum ProfileCreationOperationType {
    NON_SUPERVISED_PROFILE_CREATION,
    NO_CREATION_IN_PROGRESS
  };

  // Callback for the "requestDefaultProfileIcons" message.
  // Sends the array of default profile icon URLs to WebUI.
  void RequestDefaultProfileIcons(const base::ListValue* args);

  // Asynchronously creates and initializes a new profile.
  // The arguments are as follows:
  //   0: name (string)
  //   1: icon (string)
  //   2: a flag stating whether we should create a profile desktop shortcut
  //      (optional, boolean)
  void CreateProfile(const base::ListValue* args);

  // If a local error occurs during profile creation, then show an appropriate
  // error message. Otherwise, update the UI as the final task after a new
  // profile has been created.
  void OnProfileCreated(bool create_shortcut,
                        Profile* profile,
                        Profile::CreateStatus status);

  void HandleProfileCreationSuccess(bool create_shortcut,
                                    Profile* profile);

  // Creates desktop shortcut and updates the UI to indicate success
  // when creating a profile.
  void CreateShortcutAndShowSuccess(bool create_shortcut,
                                    Profile* profile);

  // This callback is run after a new browser (but not the window) has been
  // created for the new profile.
  void OnBrowserReadyCallback(Profile* profile, Profile::CreateStatus status);

  // Updates the UI to show an error when creating a profile.
  void ShowProfileCreationError(Profile* profile, const base::string16& error);

  // Records UMA histograms relevant to profile creation.
  void RecordProfileCreationMetrics(Profile::CreateStatus status);

  base::string16 GetProfileCreationErrorMessageLocal() const;

  base::Value GetWebUIListenerName(ProfileCreationStatus status) const;

  // Used to allow canceling a profile creation (particularly a supervised-user
  // registration) in progress. Set when profile creation is begun, and
  // cleared when all the callbacks have been run and creation is complete.
  base::FilePath profile_path_being_created_;

  // Used to track how long profile creation takes.
  base::TimeTicks profile_creation_start_time_;

  // Indicates the type of the in progress profile creation operation.
  // The value is only relevant while we are creating/importing a profile.
  ProfileCreationOperationType profile_creation_type_ = NO_CREATION_IN_PROGRESS;

  base::WeakPtrFactory<SigninCreateProfileHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninCreateProfileHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_
