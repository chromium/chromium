// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "components/session_manager/core/session_manager.h"

// static
drive::DriveIntegrationService*
ProjectorDriveFsProvider::GetActiveDriveIntegrationService() {
  return drive::DriveIntegrationServiceFactory::FindForProfile(
      ProfileManager::GetActiveUserProfile());
}

// static
bool ProjectorDriveFsProvider::IsDriveFsMounted() {
  drive::DriveIntegrationService* integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  return integration_service && integration_service->IsMounted();
}

// static
bool ProjectorDriveFsProvider::IsDriveFsMountFailed() {
  drive::DriveIntegrationService* integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  return integration_service && integration_service->mount_failed();
}

// static
base::FilePath ProjectorDriveFsProvider::GetDriveFsMountPointPath() {
  drive::DriveIntegrationService* integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  return integration_service ? integration_service->GetMountPointPath()
                             : base::FilePath();
}

ProjectorDriveFsProvider::ProjectorDriveFsProvider(
    OnDriveFsObservationChangeCallback on_drivefs_observation_change)
    : on_drivefs_observation_change_(on_drivefs_observation_change) {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager)
    session_state_observation_.Observe(user_manager);
}

ProjectorDriveFsProvider::~ProjectorDriveFsProvider() = default;

void ProjectorDriveFsProvider::OnUserProfileLoaded(
    const AccountId& account_id) {
  OnProfileSwitch();
}

void ProjectorDriveFsProvider::ActiveUserChanged(
    user_manager::User* active_user) {
  // After user login, the first ActiveUserChanged() might be called before
  // profile is loaded.
  if (active_user->is_profile_created())
    OnProfileSwitch();
}

void ProjectorDriveFsProvider::OnProfileSwitch() {
  auto* profile = ProfileManager::GetActiveUserProfile();
  if (!IsProjectorAllowedForProfile(profile))
    return;

  on_drivefs_observation_change_.Run();
}
