// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"

class PrefService;

namespace syncer {
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

using IsAccountStore = base::StrongAlias<class IsAccountStoreTag, bool>;

using metrics_util::GaiaPasswordHashChange;

class AffiliatedMatchHelper;
class PasswordStoreConsumer;

// Partial, cross-platform implementation for storing form passwords.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// PasswordStoreSync is a hidden base class because only PasswordSyncBridge
// needs to access these methods.
class PasswordStore : public PasswordStoreInterface {
 public:
  // Used to notify that unsynced credentials are about to be deleted.
  class UnsyncedCredentialsDeletionNotifier {
   public:
    // Should be called from the UI thread.
    virtual void Notify(std::vector<PasswordForm>) = 0;
    virtual ~UnsyncedCredentialsDeletionNotifier() = default;
    virtual base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() = 0;
  };

  explicit PasswordStore(std::unique_ptr<PasswordStoreBackend> backend);

  // Always call this too on the UI thread.
  // TODO(crbug.bom/1218413): Move initialization into the core interface, too.
  bool Init(
      PrefService* prefs,
      base::RepeatingClosure sync_enabled_or_disabled_cb = base::DoNothing());

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // Sets the affiliation-based match |helper| that will be used by subsequent
  // GetLogins() calls to return credentials stored not only for the requested
  // sign-on realm, but also for affiliated Android applications and Web realms.
  // If |helper| is null, clears the the currently set helper if any. Unless a
  // helper is set, affiliation-based matching is disabled. The passed |helper|
  // must already be initialized if it is non-null.
  // TODO(crbug.bom/1218413): Inject into constructor or `Init()` instead.
  void SetAffiliatedMatchHelper(std::unique_ptr<AffiliatedMatchHelper> helper);
  AffiliatedMatchHelper* affiliated_match_helper() const {
    return affiliated_match_helper_.get();
  }

  // PasswordStoreInterface:
  bool IsAbleToSavePasswords() const override;
  void AddLogin(const PasswordForm& form) override;
  void UpdateLogin(const PasswordForm& form) override;
  void UpdateLoginWithPrimaryKey(const PasswordForm& new_form,
                                 const PasswordForm& old_primary_key) override;
  void RemoveLogin(const PasswordForm& form) override;
  void RemoveLoginsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion,
      base::OnceCallback<void(bool)> sync_completion =
          base::NullCallback()) override;
  void RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> completion) override;
  void DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  void Unblocklist(const PasswordFormDigest& form_digest,
                   base::OnceClosure completion) override;
  void GetLogins(const PasswordFormDigest& form,
                 PasswordStoreConsumer* consumer) override;
  void GetAutofillableLogins(PasswordStoreConsumer* consumer) override;
  void GetAllLogins(PasswordStoreConsumer* consumer) override;
  void GetAllLoginsWithAffiliationAndBrandingInformation(
      PasswordStoreConsumer* consumer) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  PasswordStoreBackend* GetBackendForTesting() override;

  // Reports usage metrics for the database. |sync_username|, and
  // |custom_passphrase_sync_enabled|, and |is_under_advanced_protection|
  // determine some of the UMA stats that may be reported.
  virtual void ReportMetrics(const std::string& sync_username,
                             bool custom_passphrase_sync_enabled,
                             bool is_under_advanced_protection);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(base::OnceClosure task);

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

  // Status of PasswordStore::Init().
  enum class InitStatus {
    // Initialization status is still not determined (init hasn't started or
    // finished yet).
    kUnknown,
    // Initialization is successfully finished.
    kSuccess,
    // There was an error during initialization and PasswordStore is not ready
    // to save or get passwords.
    // Removing passwords may still work.
    kFailure,
  };

  // TODO(crbug.com/1217071): Remove when local backend doesn't inherit from
  // this class anymore.
  PasswordStore();
  ~PasswordStore() override;

  // Create a TaskRunner to be saved in |background_task_runner_|.
  virtual scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const;

  // Methods below will be run in PasswordStore's own sequence.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username,
                                 bool custom_passphrase_sync_enabled,
                                 BulkCheckDone bulk_check_done);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() const {
    return main_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }

  // This member is called to perform the actual interaction with the storage.
  // TODO(crbug.com/1217071): Make private std::unique_ptr as soon as the
  // backend is passed into the store instead of it being the store(_impl).
  PasswordStoreBackend* backend_ = nullptr;

 private:
  using InsecureCredentialsTask =
      base::OnceCallback<std::vector<InsecureCredential>()>;

  // Called on the main thread after initialization is completed.
  // |success| is true if initialization was successful. Sets the
  // |init_status_|.
  void OnInitCompleted(bool success);

  // Notifies observers that password store data may have been changed.
  void NotifyLoginsChangedOnMainSequence(
      const PasswordStoreChangeList& changes);

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChangedOnMainSequence(). Note that there is
  // no guarantee that the called method will actually modify the password store
  // data.
  void UnblocklistInternal(base::OnceClosure completion,
                           std::vector<std::unique_ptr<PasswordForm>> forms);

  // Retrieves and fills in affiliation and branding information for Android
  // credentials in |forms| and invokes |callback| with the result. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(LoginsReply callback,
                                               LoginsResult forms);

  // The local backend is currently a ref-counted type because it still inherits
  // from PasswordStore and this would be a self reference. So, if `this` is an
  // instance of PasswordStoreImpl, this member is not used.
  //
  // If the backend is injected via the public constructor, this backend_deleter
  // owns the instance and deletes it on destruction. Once backend_ is a
  // unique_ptr, too, this deleter can simply be removed.
  // TODO(crbug.com/1217071): Remove once once backend_ is a unique_ptr.
  std::unique_ptr<PasswordStoreBackend> backend_deleter_ = nullptr;

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The observers.
  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

  PrefService* prefs_ = nullptr;

  bool shutdown_called_ = false;

  InitStatus init_status_ = InitStatus::kUnknown;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

// For testing only.
#if defined(UNIT_TEST)
inline std::ostream& operator<<(std::ostream& os,
                                const PasswordFormDigest& digest) {
  return os << "PasswordFormDigest(scheme: " << digest.scheme
            << ", signon_realm: " << digest.signon_realm
            << ", url: " << digest.url << ")";
}
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
