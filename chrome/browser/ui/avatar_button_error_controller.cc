// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/avatar_button_error_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"

AvatarButtonErrorController::AvatarButtonErrorController(
    AvatarButtonErrorControllerDelegate* delegate,
    Profile* profile)
    : delegate_(delegate),
      avatar_signin_error_controller_(profile, this),
      avatar_sync_error_controller_(profile, this),
      has_signin_error_(avatar_signin_error_controller_.HasSigninError()),
      has_sync_error_(avatar_sync_error_controller_.HasSyncError()) {}

AvatarButtonErrorController::~AvatarButtonErrorController() {}

void AvatarButtonErrorController::UpdateSigninError(bool has_signin_error) {
  bool had_error = HasAvatarError();
  has_signin_error_ = has_signin_error;
  if (had_error != HasAvatarError())
    delegate_->OnAvatarErrorChanged();
}

void AvatarButtonErrorController::UpdateSyncError(bool has_sync_error) {
  has_sync_error_ = has_sync_error;
  // Always notify observers, so they can handle errors differently.
  delegate_->OnAvatarErrorChanged();
}

AvatarButtonErrorController::SigninErrorObserver::SigninErrorObserver(
    Profile* profile,
    AvatarButtonErrorController* avatar_button_error_controller)
    : profile_(profile),
      avatar_button_error_controller_(avatar_button_error_controller) {
  SigninErrorController* signin_error_controller =
      profiles::GetSigninErrorController(profile_);
  if (signin_error_controller)
    signin_error_controller->AddObserver(this);
}

AvatarButtonErrorController::SigninErrorObserver::~SigninErrorObserver() {
  SigninErrorController* signin_error_controller =
      profiles::GetSigninErrorController(profile_);
  if (signin_error_controller)
    signin_error_controller->RemoveObserver(this);
}

void AvatarButtonErrorController::SigninErrorObserver::OnErrorChanged() {
  avatar_button_error_controller_->UpdateSigninError(HasSigninError());
}

bool AvatarButtonErrorController::SigninErrorObserver::HasSigninError() {
  const SigninErrorController* signin_error_controller =
      profiles::GetSigninErrorController(profile_);
  return signin_error_controller && signin_error_controller->HasError();
}

AvatarButtonErrorController::SyncErrorObserver::SyncErrorObserver(
    Profile* profile,
    AvatarButtonErrorController* avatar_button_error_controller)
    : profile_(profile),
      avatar_button_error_controller_(avatar_button_error_controller) {
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_observer_.Add(sync_service);
}

AvatarButtonErrorController::SyncErrorObserver::~SyncErrorObserver() = default;

void AvatarButtonErrorController::SyncErrorObserver::OnStateChanged(
    syncer::SyncService* sync_service) {
  avatar_button_error_controller_->UpdateSyncError(HasSyncError());
}

bool AvatarButtonErrorController::SyncErrorObserver::HasSyncError() {
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    return sync_service->HasUnrecoverableError() ||
           sync_ui_util::ShouldShowPassphraseError(sync_service) ||
           sync_ui_util::ShouldShowSyncKeysMissingError(sync_service) ||
           sync_ui_util::ShouldRequestSyncConfirmation(sync_service);
  }
  return false;
}
