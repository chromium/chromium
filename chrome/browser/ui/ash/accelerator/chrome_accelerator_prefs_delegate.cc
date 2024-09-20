// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accelerator/chrome_accelerator_prefs_delegate.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user_manager.h"

bool ChromeAcceleratorPrefsDelegate::IsUserEnterpriseManaged() const {
  // Ensure the user is logged in.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return false;
  }
  // Check if the user is managed.
  return ProfileManager::GetPrimaryUserProfile()
      ->GetProfilePolicyConnector()
      ->IsManaged();
}
