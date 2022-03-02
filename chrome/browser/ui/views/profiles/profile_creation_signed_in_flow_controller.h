// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

struct AccountInfo;
struct CoreAccountInfo;
class Profile;

// Class responsible for the part of the profile creation flow where the user is
// signed in (most importantly offering sync).
class ProfileCreationSignedInFlowController
    : public ProfilePickerSignedInFlowController,
      public signin::IdentityManager::Observer {
 public:
  ProfileCreationSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      absl::optional<SkColor> profile_color,
      base::TimeDelta extended_account_info_timeout,
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
  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // Helper functions to deal with the lack of extended account info.
  void OnExtendedAccountInfoTimeout(const CoreAccountInfo& account);
  void OnProfileNameAvailable();

  void FinishAndOpenBrowserImpl(ProfilePicker::BrowserOpenedCallback callback);

  // Finishes the flow by finalizing the profile and continuing the SAML
  // sign-in in a browser window.
  void FinishAndOpenBrowserForSAML();
  void OnSignInContentsFreedUp();

  // Internal callback to finish the last steps of the signed-in creation
  // flow.
  void OnBrowserOpened(
      ProfilePicker::BrowserOpenedCallback finish_flow_callback,
      Profile* profile_with_browser_opened);

  // For finishing the profile creation flow, the extended account info is
  // needed (for properly naming the new profile). After a timeout, a fallback
  // name is used, instead, to unblock the flow.
  base::TimeDelta extended_account_info_timeout_;

  // Stores whether this is profile creation for saml sign-in (that skips most
  // of the logic).
  const bool is_saml_;

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finished_ = false;

  std::u16string name_for_signed_in_profile_;
  base::OnceClosure on_profile_name_available_;

  base::CancelableOnceClosure extended_account_info_timeout_closure_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfileCreationSignedInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CREATION_SIGNED_IN_FLOW_CONTROLLER_H_
