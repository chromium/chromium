// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"

class Profile;

// Class responsible for the part of the profile creation flow where the user is
// signed in (most importantly offering sync).
class ProfileCreationSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  ProfileCreationSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      absl::optional<SkColor> profile_color,
      bool is_saml);
  ~ProfileCreationSignedInFlowController() override;
  ProfileCreationSignedInFlowController(
      const ProfilePickerSignedInFlowController&) = delete;
  ProfileCreationSignedInFlowController& operator=(
      const ProfilePickerSignedInFlowController&) = delete;

  // ProfilePickerSignedInFlowController:
  void Init() override;
  void Cancel() override;
  void FinishAndOpenBrowser(
      ProfilePicker::BrowserOpenedCallback callback) override;

 private:
  // Finishes the non-SAML flow, registering customisation-related callbacks if
  // no `callback` is povided.
  void FinishAndOpenBrowserImpl(ProfilePicker::BrowserOpenedCallback callback);

  // Finishes the SAML flow by continuing the sign-in in a browser window.
  void FinishAndOpenBrowserForSAML();
  void OnSignInContentsFreedUp();

  // Shared helper. Opens a new browser window, closes the picker and runs
  // `callback` in the opened window.
  void ExitPickerAndRunInNewBrowser(
      ProfilePicker::BrowserOpenedCallback callback);

  // Internal callback to finish the last steps of the signed-in creation
  // flow.
  void OnBrowserOpened(
      ProfilePicker::BrowserOpenedCallback finish_flow_callback,
      Profile* profile_with_browser_opened);

  // Stores whether this is profile creation for saml sign-in (that skips most
  // of the logic).
  const bool is_saml_;

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finished_ = false;

  std::unique_ptr<ProfileNameResolver> profile_name_resolver_;

  base::WeakPtrFactory<ProfileCreationSignedInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_
