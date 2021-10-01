// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TURN_SYNC_ON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TURN_SYNC_ON_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class Profile;
class SigninUIError;

// Handles turning on sync for signed-in profile creation flow, embedded in the
// profile picker.
class ProfilePickerTurnSyncOnDelegate : public DiceTurnSyncOnHelper::Delegate,
                                        public LoginUIService::Observer {
 public:
  ProfilePickerTurnSyncOnDelegate(
      base::WeakPtr<ProfilePickerSignedInFlowController> controller,
      Profile* profile);
  ~ProfilePickerTurnSyncOnDelegate() override;
  ProfilePickerTurnSyncOnDelegate(const ProfilePickerTurnSyncOnDelegate&) =
      delete;
  ProfilePickerTurnSyncOnDelegate& operator=(
      const ProfilePickerTurnSyncOnDelegate&) = delete;

 private:
  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  // Shows the sync confirmation screen in the profile creation flow.
  void ShowSyncConfirmationScreen();

  // When ShowSync(Disabled)Confirmation() gets called, this must eventually get
  // called exactly once in all code branches. Handles the callback and reports
  // the metrics.
  void FinishSyncConfirmation(
      LoginUIService::SyncConfirmationUIClosedResult result,
      absl::optional<ProfileMetrics::ProfileAddSignInFlowOutcome> outcome);

  // Shows the enterprise welcome screen.
  void ShowEnterpriseWelcome(EnterpriseProfileWelcomeUI::ScreenType type);
  void OnEnterpriseWelcomeClosed(EnterpriseProfileWelcomeUI::ScreenType type,
                                 bool proceed);

  // Controls the sign-in flow. Is not guaranteed to outlive this object (gets
  // destroyed when the flow window closes).
  base::WeakPtr<ProfilePickerSignedInFlowController> controller_;

  raw_ptr<Profile> profile_;
  bool enterprise_account_ = false;
  bool sync_disabled_ = false;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TURN_SYNC_ON_DELEGATE_H_
