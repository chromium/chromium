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
class ModelTypeControllerDelegate;
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

using IsAccountStore = base::StrongAlias<class IsAccountStoreTag, bool>;

using metrics_util::GaiaPasswordHashChange;

class AffiliatedMatchHelper;
class PasswordStoreConsumer;
class InsecureCredentialsConsumer;
class PasswordStoreConsumer;
struct FieldInfo;

// Partial, cross-platform implementation for storing form passwords.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// PasswordStoreSync is a hidden base class because only PasswordSyncBridge
// needs to access these methods.
class PasswordStore : public PasswordStoreInterface,
                      protected SmartBubbleStatsStore {
 public:
  // Used to notify that unsynced credentials are about to be deleted.
  class UnsyncedCredentialsDeletionNotifier {
   public:
    // Should be called from the UI thread.
    virtual void Notify(std::vector<PasswordForm>) = 0;
    virtual ~UnsyncedCredentialsDeletionNotifier() = default;
    virtual base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() = 0;
  };

  PasswordStore();

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
  void GetLoginsByPassword(const std::u16string& plain_text_password,
                           PasswordStoreConsumer* consumer) override;
  void GetAutofillableLogins(PasswordStoreConsumer* consumer) override;
  void GetAllLogins(PasswordStoreConsumer* consumer) override;
  void GetAllLoginsWithAffiliationAndBrandingInformation(
      PasswordStoreConsumer* consumer) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;


  // Reports usage metrics for the database. |sync_username|, and
  // |custom_passphrase_sync_enabled|, and |is_under_advanced_protection|
  // determine some of the UMA stats that may be reported.
  virtual void ReportMetrics(const std::string& sync_username,
                             bool custom_passphrase_sync_enabled,
                             bool is_under_advanced_protection);

  // Adds information about credentials issue on
  // |insecure_credential.url| for |insecure_credential.username|. The
  // first |insecure_credential.create_time| is kept, so if the record for
  // given url and username already exists, the new one will be ignored.
  void AddInsecureCredential(const InsecureCredential& insecure_credential);

  // Removes information about insecure credentials on |signon_realm| for
  // |username|.
  void RemoveInsecureCredentials(const std::string& signon_realm,
                                 const std::u16string& username,
                                 RemoveInsecureCredentialsReason reason);

  // Retrieves all insecure credentials and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  void GetAllInsecureCredentials(InsecureCredentialsConsumer* consumer);

  // Returns all the insecure credentials for a given site. This list also
  // includes Android affiliated credentials.
  void GetMatchingInsecureCredentials(const std::string& signon_realm,
                                      InsecureCredentialsConsumer* consumer);

  // Adds information about field. If the record for given form_signature and
  // field_signature already exists, the new one will be ignored.
  void AddFieldInfo(const FieldInfo& field_info);

  // Retrieves all field info and notifies |consumer| on completion. The request
  // will be cancelled if the consumer is destroyed.
  void GetAllFieldInfo(PasswordStoreConsumer* consumer);

  // Removes all leaked credentials in the given date range. If |completion| is
  // not null, it will be posted to the |main_task_runner_| after deletions have
  // been completed. Should be called on the UI thread.
  void RemoveFieldInfoByTime(base::Time remove_begin,
                             base::Time remove_end,
                             base::OnceClosure completion);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(base::OnceClosure task);

  // For sync codebase only: instantiates a proxy controller delegate to
  // interact with PasswordSyncBridge. Must be called from the UI thread.
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate();

  // Sets |deletion_notifier_|. Must not pass a nullptr.
  virtual void SetUnsyncedCredentialsDeletionNotifier(
      std::unique_ptr<UnsyncedCredentialsDeletionNotifier>
          deletion_notifier) = 0;

 protected:
  using LoginsTask = base::OnceCallback<LoginsResult()>;
  using LoginsResultProcessor =
      base::OnceCallback<void(LoginsReply, LoginsResult)>;

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

  ~PasswordStore() override;

  // SmartBubbleStatsStore:
  void AddSiteStats(const InteractionsStats& stats) override;
  void RemoveSiteStats(const GURL& origin_domain) override;
  void GetSiteStats(const GURL& origin_domain,
                    PasswordStoreConsumer* consumer) override;
  void RemoveStatisticsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion) override;

  // Create a TaskRunner to be saved in |background_task_runner_|.
  virtual scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const;

  // Methods below will be run in PasswordStore's own sequence.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username,
                                 bool custom_passphrase_sync_enabled,
                                 BulkCheckDone bulk_check_done) = 0;

  // Synchronous implementation to remove the statistics.
  virtual bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to disable auto sign-in.
  virtual PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) = 0;

  // Finds and returns all PasswordForms with the same signon_realm as |form|,
  // or with a signon_realm that is a PSL-match to that of |form|.
  virtual std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const PasswordFormDigest& form) = 0;

  // Finds and returns all not-blocklisted PasswordForms with the specified
  // |plain_text_password| stored in the credential database.
  virtual std::vector<std::unique_ptr<PasswordForm>>
  FillMatchingLoginsByPassword(const std::u16string& plain_text_password) = 0;

  // Synchronous implementation for manipulating with statistics.
  virtual void AddSiteStatsImpl(const InteractionsStats& stats) = 0;
  virtual void RemoveSiteStatsImpl(const GURL& origin_domain) = 0;
  virtual std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) = 0;

  // Synchronous implementation for manipulating with information about
  // insecure credentials.
  // Returns PasswordStoreChangeList for the updated password forms.
  virtual PasswordStoreChangeList AddInsecureCredentialImpl(
      const InsecureCredential& insecure_credential) = 0;
  virtual PasswordStoreChangeList RemoveInsecureCredentialsImpl(
      const std::string& signon_realm,
      const std::u16string& username,
      RemoveInsecureCredentialsReason reason) = 0;
  virtual std::vector<InsecureCredential> GetAllInsecureCredentialsImpl() = 0;
  virtual std::vector<InsecureCredential> GetMatchingInsecureCredentialsImpl(
      const std::string& signon_realm) = 0;

  // Synchronous implementation for manipulating with information about field
  // info.
  virtual void AddFieldInfoImpl(const FieldInfo& field_info) = 0;
  virtual std::vector<FieldInfo> GetAllFieldInfoImpl() = 0;
  virtual void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                         base::Time remove_end) = 0;

  // Synchronous implementation provided by subclasses to check whether the
  // store is empty.
  virtual bool IsEmpty() = 0;

  // Returns the sync controller delegate for syncing passwords. It must be
  // called on the background sequence.
  // TODO(crbug.bom/1226042): Remove this after fully switching to the
  // PasswordStoreInterface.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence() = 0;

  // Invokes callback and notifies observers if there was a change to the list
  // of insecure passwords. It also informs Sync about the updated password
  // forms to sync up the changes about insecure credentials.
  void InvokeAndNotifyAboutInsecureCredentialsChange(
      base::OnceCallback<PasswordStoreChangeList()> callback);

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
  using StatsResult = std::vector<InteractionsStats>;
  using StatsTask = base::OnceCallback<StatsResult()>;

  using InsecureCredentialsTask =
      base::OnceCallback<std::vector<InsecureCredential>()>;

  // Called on the main thread after initialization is completed.
  // |success| is true if initialization was successful. Sets the
  // |init_status_|.
  void OnInitCompleted(bool success);

  // Notifies observers that password store data may have been changed.
  void NotifyLoginsChangedOnMainSequence(
      const PasswordStoreChangeList& changes);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetPasswordStoreResults() on the caller's thread with
  // the result.
  void PostLoginsTaskAndReplyToConsumerWithResult(
      PasswordStoreConsumer* consumer,
      LoginsTask task);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetPasswordStoreResults() on the caller's thread with
  // the result, after it was post-processed by |processor|.
  // |trace_name| is the trace to be closed before calling the consumer.
  void PostLoginsTaskAndReplyToConsumerWithProcessedResult(
      const char* trace_name,
      PasswordStoreConsumer* consumer,
      LoginsTask task,
      LoginsResultProcessor processor);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetSiteStatistics() on the caller's thread with the
  // result.
  void PostStatsTaskAndReplyToConsumerWithResult(
      PasswordStoreConsumer* consumer,
      StatsTask task);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetInsecureCredentials() on the caller's thread
  // with the result.
  void PostInsecureCredentialsTaskAndReplyToConsumerWithResult(
      InsecureCredentialsConsumer* consumer,
      InsecureCredentialsTask task);

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChangedOnMainSequence(). Note that there is
  // no guarantee that the called method will actually modify the password store
  // data.
  void UnblocklistInternal(base::OnceClosure completion,
                           std::vector<std::unique_ptr<PasswordForm>> forms);

  void RemoveStatisticsByOriginAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion);
  void DisableAutoSignInForOriginsInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion);
  PasswordStoreChangeList RemoveCompromisedCredentialsByUrlAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end,
      base::OnceClosure completion);

  void RemoveFieldInfoByTimeInternal(base::Time remove_begin,
                                     base::Time remove_end,
                                     base::OnceClosure completion);

  // Finds all PasswordForms with a signon_realm that is equal to, or is a
  // PSL-match to that of |form|, and takes care of notifying the consumer with
  // the results when done.
  // Note: subclasses should implement FillMatchingLogins() instead.
  std::vector<std::unique_ptr<PasswordForm>> GetLoginsImpl(
      const PasswordFormDigest& form);

  // Finds all credentials with the specified |plain_text_password|.
  // Note: subclasses should implement FillMatchingLoginsByPassword() instead.
  std::vector<std::unique_ptr<PasswordForm>> GetLoginsByPasswordImpl(
      const std::u16string& plain_text_password);

  // Extended version of GetMatchingInsecureCredentialsImpl that also returns
  // credentials stored for the specified affiliated Android applications or Web
  // realms.
  std::vector<InsecureCredential> GetInsecureCredentialsWithAffiliationsImpl(
      const std::string& signon_realm,
      const std::vector<std::string>& additional_affiliated_realms);

  // Retrieves and fills in affiliation and branding information for Android
  // credentials in |forms| and invokes |callback| with the result. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(LoginsReply callback,
                                               LoginsResult forms);

  // Schedules GetInsecureCredentialsWithAffiliationsImpl() to be run on the
  // background sequence.
  void ScheduleGetInsecureCredentialsWithAffiliations(
      base::WeakPtr<InsecureCredentialsConsumer> consumer,
      const std::string& signon_realm,
      const std::vector<std::string>& additional_affiliated_realms);

  // Retrieves the currently stored form, if any, with the same primary key as
  // |form|, that is, with the same signon_realm, url, username_element,
  // username_value and password_element attributes. To be called on the
  // background sequence.
  std::unique_ptr<PasswordForm> GetLoginImpl(const PasswordForm& primary_key);

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
