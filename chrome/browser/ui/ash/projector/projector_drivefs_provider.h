// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_DRIVEFS_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_DRIVEFS_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace session_manager {
class SessionManager;
}  // namespace session_manager

// Provides the DriveFs service for a given user. Encapsulates the logic to
// observe the user session and trigger the callback when the active user
// changes.
class ProjectorDriveFsProvider
    : public session_manager::SessionManagerObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // All of these are keyed on the user whose DriveFS is being asked about;
  // callers take the AccountId from OnDriveFsObservationChangeCallback.
  static drive::DriveIntegrationService* GetDriveIntegrationService(
      const AccountId& account_id);
  static bool IsDriveFsMounted(const AccountId& account_id);
  static bool IsDriveFsMountFailed(const AccountId& account_id);
  static base::FilePath GetDriveFsMountPointPath(const AccountId& account_id);

  // Runs with the AccountId of the user whose DriveFS should now be observed.
  using OnDriveFsObservationChangeCallback =
      base::RepeatingCallback<void(const AccountId&)>;
  explicit ProjectorDriveFsProvider(
      OnDriveFsObservationChangeCallback on_drivefs_observation_change);
  ProjectorDriveFsProvider(const ProjectorDriveFsProvider&) = delete;
  ProjectorDriveFsProvider& operator=(const ProjectorDriveFsProvider&) = delete;
  ~ProjectorDriveFsProvider() override;

 private:
  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void OnActiveUserSwitch(const AccountId& account_id);

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      session_state_observation_{this};

  OnDriveFsObservationChangeCallback on_drivefs_observation_change_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_DRIVEFS_PROVIDER_H_
