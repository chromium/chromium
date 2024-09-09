// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_TRACKER_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_TRACKER_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class AccountCapabilities;
class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Image;
}

namespace signin {
class IdentityManager;
void SimulateSuccessfulFetchOfAccountInfo(IdentityManager*,
                                          const CoreAccountId&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&);
void SimulateAccountImageFetch(signin::IdentityManager*,
                               const CoreAccountId&,
                               const std::string& image_url_with_size,
                               const gfx::Image&);
}  // namespace signin

// Retrieves and caches GAIA information about Google Accounts.
class AccountTrackerService {
 public:
  typedef base::RepeatingCallback<void(const AccountInfo& info)>
      AccountInfoCallback;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Possible values for the kAccountIdMigrationState preference.
  // Keep in sync with OAuth2LoginAccountRevokedMigrationState histogram enum.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // TODO(crbug.com/40268200): Remove the migration code after enough users
  // have migrated.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED = 0,
    MIGRATION_IN_PROGRESS = 1,
    MIGRATION_DONE = 2,
    NUM_MIGRATION_STATES
  };
#endif

  AccountTrackerService();

  AccountTrackerService(const AccountTrackerService&) = delete;
  AccountTrackerService& operator=(const AccountTrackerService&) = delete;

  ~AccountTrackerService();

  // Registers the preferences used by AccountTrackerService.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initializes the list of accounts from |pref_service| and load images from
  // |user_data_dir|. If |user_data_dir| is empty, images will not be saved to
  // nor loaded from disk.
  void Initialize(PrefService* pref_service, base::FilePath user_data_dir);

  // Returns the list of known accounts and for which gaia IDs
  // have been fetched.
  std::vector<AccountInfo> GetAccounts() const;
  AccountInfo GetAccountInfo(const CoreAccountId& account_id) const;
  AccountInfo FindAccountInfoByGaiaId(const std::string& gaia_id) const;
  AccountInfo FindAccountInfoByEmail(const std::string& email) const;

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  CoreAccountId PickAccountIdForAccount(const std::string& gaia,
                                        const std::string& email) const;

  // Seeds the account whose account_id is given by PickAccountIdForAccount()
  // with its corresponding gaia id and email address.  Returns the same
  // value PickAccountIdForAccount() when given the same arguments.
  CoreAccountId SeedAccountInfo(
      const std::string& gaia,
      const std::string& email,
      signin_metrics::AccessPoint access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  // Seeds the account represented by |info|. If the account is already tracked
  // and compatible, the empty fields will be updated with values from |info|.
  // If after the update IsValid() is true, OnAccountUpdated will be fired.
  CoreAccountId SeedAccountInfo(AccountInfo info);

  // Seeds the accounts with |core_account_infos|. The primary account id is
  // passed to keep it from getting removed.
  void SeedAccountsInfo(const std::vector<CoreAccountInfo>& core_account_infos,
                        const std::optional<CoreAccountId>& primary_account_id,
                        bool should_remove_stale_accounts);

  // Sets whether the account is a Unicorn account.
  void SetIsChildAccount(const CoreAccountId& account_id,
                         bool is_child_account);

  // Sets whether the account is under advanced protection.
  void SetIsAdvancedProtectionAccount(const CoreAccountId& account_id,
                                      bool is_under_advanced_protection);

  void RemoveAccount(const CoreAccountId& account_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AccountIdMigrationState GetMigrationState() const;
  void SetMigrationDone();
#endif

  // If set, this callback will be invoked whenever the details of a tracked
  // account changes (e.g. account's info, image, |is_child_account|...).
  void SetOnAccountUpdatedCallback(AccountInfoCallback callback);

  // If set, this callback will be invoked whenever an existing account with a
  // valid GaiaId gets removed from |accounts_| (i.e. stops being tracked).
  void SetOnAccountRemovedCallback(AccountInfoCallback callback);

  // Flushes the account changes to disk. The flush happens asynchronously and
  // this function does not block on disk IO.
  void CommitPendingAccountChanges();

  // Only used in tests to simulate a restart of the service. Accounts are
  // reloaded.
  void ResetForTesting();

 protected:
  // Available to be called in tests.
  void SetAccountInfoFromUserInfo(const CoreAccountId& account_id,
                                  const base::Value::Dict& user_info);

  // Updates the account image. Does nothing if |account_id| does not exist in
  // |accounts_|.
  void SetAccountImage(const CoreAccountId& account_id,
                       const std::string& image_url_with_size,
                       const gfx::Image& image);

  // Updates the account capabilities in AccountInfo for |account_id|. Does
  // nothing if |account_id| does not exist in |accounts_|.
  void SetAccountCapabilities(const CoreAccountId& account_id,
                              const AccountCapabilities& account_capabilities);

 private:
  friend class AccountFetcherService;
  friend class AccountTrackerServiceTest;
  friend void signin::SimulateSuccessfulFetchOfAccountInfo(
      signin::IdentityManager*,
      const CoreAccountId&,
      const std::string&,
      const std::string&,
      const std::string&,
      const std::string&,
      const std::string&,
      const std::string&,
      const std::string&);
  friend void signin::SimulateAccountImageFetch(signin::IdentityManager*,
                                                const CoreAccountId&,
                                                const std::string&,
                                                const gfx::Image&);

  void NotifyAccountUpdated(const AccountInfo& account_info);
  void NotifyAccountRemoved(const AccountInfo& account_info);

  // Start tracking `account_id` (`account_id` must not be empty).
  void StartTrackingAccount(const CoreAccountId& account_id);
  bool IsTrackingAccount(const CoreAccountId& account_id);
  void StopTrackingAccount(const CoreAccountId& account_id);

  // Load the current state of the account info from the preferences file.
  void LoadFromPrefs();
  void SaveToPrefs(const AccountInfo& account);
  void RemoveFromPrefs(const AccountInfo& account);

  // Used to load/save account images from/to disc.
  base::FilePath GetImagePathFor(const CoreAccountId& account_id);
  void OnAccountImageLoaded(const CoreAccountId& account_id, gfx::Image image);
  void LoadAccountImagesFromDisk();
  void SaveAccountImageToDisk(const CoreAccountId& account_id,
                              const gfx::Image& image,
                              const std::string& image_url_with_size);
  void OnAccountImageUpdated(const CoreAccountId& account_id,
                             const std::string& image_url_with_size,
                             bool success);
  void RemoveAccountImageFromDisk(const CoreAccountId& account_id);

  // Returns whether the accounts are all keyed by gaia id. This should
  // be the case when the migration state is set to MIGRATION_DONE.
  bool AreAllAccountsMigrated() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Migrate accounts to be keyed by gaia id instead of normalized email.
  // Requires that the migration state is set to MIGRATION_IN_PROGRESS.
  void MigrateToGaiaId();

  // Computes the new migration state. The state is saved to preference
  // before performing the migration in order to support resuming the
  // migration if necessary during the next load.
  AccountIdMigrationState ComputeNewMigrationState() const;

  // Updates the migration state in the preferences.
  void SetMigrationState(AccountIdMigrationState state);

  // Returns the saved migration state in the preferences.
  static AccountIdMigrationState GetMigrationState(
      const PrefService* pref_service);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Update the child status on the provided account.
  // This does not notify observers, or persist updates to disk - the caller
  // is responsible for doing so.
  // Returns true if the child status was modified, false otherwise.
  bool UpdateAccountInfoChildStatus(AccountInfo& account_info,
                                    bool is_child_account);

  raw_ptr<PrefService> pref_service_ = nullptr;  // Not owned.
  std::map<CoreAccountId, AccountInfo> accounts_;
  base::FilePath user_data_dir_;

  AccountInfoCallback on_account_updated_callback_;
  AccountInfoCallback on_account_removed_callback_;

  // Task runner used for file operations on avatar images.
  scoped_refptr<base::SequencedTaskRunner> image_storage_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to pass weak pointers of |this| to tasks created by
  // |image_storage_task_runner_|.
  base::WeakPtrFactory<AccountTrackerService> weak_factory_{this};
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_TRACKER_SERVICE_H_
