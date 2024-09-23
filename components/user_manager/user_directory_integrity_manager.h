// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
#define COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
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

  // This enum values are persisted in `LocalState`, do not remove values,
  // and only add values at the end.
  enum class CleanupStrategy {
    // Default value, that just removes (unusable) crytohome and
    // all entries in LocalState related to the user.
    kRemoveUser,
    // For owner user, removal of cryptohome would mean the loss of
    // private key used to sign device settings, so silent powerwash
    // should be performed instead.
    kSilentPowerwash,
    kMaxValue = kSilentPowerwash
  };

  explicit UserDirectoryIntegrityManager(PrefService* local_state);
  UserDirectoryIntegrityManager(const UserDirectoryIntegrityManager&) = delete;
  UserDirectoryIntegrityManager& operator=(
      const UserDirectoryIntegrityManager&) = delete;
  ~UserDirectoryIntegrityManager();

  // Mark local state that we are about to create a new user home dir.
  // The `strategy` should be used in case user creation does not finish.
  void RecordCreatingNewUser(const AccountId&, CleanupStrategy strategy);

  // Clears known user prefs after removal of an incomplete user.
  void RemoveUser(const AccountId& account_id);

  // Remove the mark previously placed in local state, meaning an auth factor
  // has been added, or an unusable user has been successfully cleaned up.
  // This doesn't clear known user prefs.
  void ClearPrefs();

  // Check if a user has been incompletely created by looking for the
  // presence of a mark associated with the user's email.
  std::optional<AccountId> GetMisconfiguredUserAccountId();
  CleanupStrategy GetMisconfiguredUserCleanupStrategy();

  bool IsUserMisconfigured(const AccountId& account_id);

 private:
  std::optional<std::string> GetMisconfiguredUserEmail();
  std::optional<AccountId> GetMisconfiguredUserAccountIdLegacy();

  const raw_ptr<PrefService> local_state_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_DIRECTORY_INTEGRITY_MANAGER_H_
