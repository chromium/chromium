// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
#define COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_

#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {

// This class is responsible for writing to local_state before a user is
// created via `MountPerformer::CreateNewUser` and clearing that record from
// local_state when an auth factor is added, via
// `AuthFactorEditor::OnAddCredential`
//
// In that small window between creating a new user and adding keys, we could
// crash, leaving us in an inconsistent state where we have a user home
// directory with no keys. This class helps detect that.
class USER_MANAGER_EXPORT UserDirectoryIntegrityManager {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit UserDirectoryIntegrityManager(PrefService* local_state);
  UserDirectoryIntegrityManager(const UserDirectoryIntegrityManager&) = delete;
  UserDirectoryIntegrityManager& operator=(
      const UserDirectoryIntegrityManager&) = delete;
  ~UserDirectoryIntegrityManager();

  // Mark local state that we are about to create a new user home dir.
  void RecordCreatingNewUser(const AccountId&);

  // Clears known user prefs after removal of an incomplete user.
  void ClearKnownUserPrefs();

  // Remove the mark previously placed in local state, meaning an auth factor
  // has been added, or an unusable user has been successfully cleaned up.
  // This doesn't clear known user prefs.
  void ClearPrefs();

  // Check if a user has been incompletely created by looking for the
  // presence of a mark associated with the user's email.
  absl::optional<AccountId> GetMisconfiguredUser();

 private:
  PrefService* const local_state_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
