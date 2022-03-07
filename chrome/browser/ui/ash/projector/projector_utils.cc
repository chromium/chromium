// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_utils.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

bool IsRealUserProfile(const Profile* profile) {
  // Return false for signin, lock screen and incognito profiles.
  return chromeos::ProfileHelper::IsRegularProfile(profile) &&
         !profile->IsOffTheRecord();
}

}  // namespace

bool IsProjectorAllowedForProfile(const Profile* profile) {
  DCHECK(profile);
  if (!IsRealUserProfile(profile))
    return false;

  auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;

  return user->HasGaiaAccount();
}

drive::DriveIntegrationService* GetDriveIntegrationServiceForActiveProfile() {
  return drive::DriveIntegrationServiceFactory::FindForProfile(
      ProfileManager::GetActiveUserProfile());
}
