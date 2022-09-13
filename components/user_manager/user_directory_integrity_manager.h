// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
#define COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_

#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager_export.h"

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

  // Remove the mark previously placed in local state, meaning an auth factor
  // has been added.
  void RecordAuthFactorAdded(const AccountId&);

  // Check if the user has been incompletely created. i.e: if the local state
  // has the given user marked in it.
  std::string GetIncompleteUser();

 private:
  PrefService* const local_state_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
