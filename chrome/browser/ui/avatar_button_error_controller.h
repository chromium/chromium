// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AVATAR_BUTTON_ERROR_CONTROLLER_H_
#define CHROME_BROWSER_UI_AVATAR_BUTTON_ERROR_CONTROLLER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class Profile;

// Keeps track of the signin and sync errors that should be exposed to the user
// in the avatar button.
class AvatarButtonErrorController {
 public:
  AvatarButtonErrorController(AvatarButtonErrorControllerDelegate* delegate,
                              Profile* profile);
  ~AvatarButtonErrorController();

  bool HasAvatarError() const { return has_signin_error_ || has_sync_error_; }

 private:
  friend class SigninErrorObserver;
  friend class SyncErrorObserver;

  // Observes signin errors and updates the error controller for the avatar
  // button accordingly.
  class SigninErrorObserver : public SigninErrorController::Observer {
   public:
    SigninErrorObserver(
        Profile* profile,
        AvatarButtonErrorController* avatar_button_error_controller);
    ~SigninErrorObserver() override;

    // SigninErrorController::Observer:
    void OnErrorChanged() override;

    bool HasSigninError();

   private:
    Profile* profile_;
    AvatarButtonErrorController* avatar_button_error_controller_;

    DISALLOW_COPY_AND_ASSIGN(SigninErrorObserver);
  };

  // Observes sync errors and updates the error controller for the avatar
  // button accordingly.
  class SyncErrorObserver : public syncer::SyncServiceObserver {
   public:
    SyncErrorObserver(
        Profile* profile,
        AvatarButtonErrorController* avatar_button_error_controller);
    ~SyncErrorObserver() override;

    // SyncServiceObserver:
    void OnStateChanged(syncer::SyncService* sync_service) override;

    bool HasSyncError();

   private:
    Profile* profile_;
    AvatarButtonErrorController* avatar_button_error_controller_;

    ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
        sync_observer_{this};

    DISALLOW_COPY_AND_ASSIGN(SyncErrorObserver);
  };

  void UpdateSigninError(bool has_signin_error);
  void UpdateSyncError(bool has_sync_error);

  AvatarButtonErrorControllerDelegate* delegate_;

  SigninErrorObserver avatar_signin_error_controller_;
  SyncErrorObserver avatar_sync_error_controller_;

  bool has_signin_error_;
  bool has_sync_error_;

  DISALLOW_COPY_AND_ASSIGN(AvatarButtonErrorController);
};

#endif  // CHROME_BROWSER_UI_AVATAR_BUTTON_ERROR_CONTROLLER_H_
