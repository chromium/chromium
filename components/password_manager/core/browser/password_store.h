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
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/model/syncable_service.h"

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#endif

class PrefService;

namespace autofill {
struct FormData;
struct PasswordForm;
}

namespace syncer {
class ModelTypeControllerDelegate;
class ProxyModelTypeControllerDelegate;
class SyncableService;
}

using StateSubscription =
    base::CallbackList<void(const std::string& username)>::Subscription;

namespace password_manager {

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
using metrics_util::GaiaPasswordHashChange;
#endif

class AffiliatedMatchHelper;
class PasswordStoreConsumer;
class PasswordLeakHistoryConsumer;
class PasswordStoreSigninNotifier;
class PasswordSyncableService;
class PasswordSyncBridge;
struct FieldInfo;
struct InteractionsStats;
struct CompromisedCredentials;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
using PasswordHashDataList = base::Optional<std::vector<PasswordHashData>>;
#endif

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// Implementations, however, should carry out most tasks asynchronously on a
// background sequence: the base class provides functionality to facilitate
// this. I/O heavy initialization should also be performed asynchronously in
// this manner. If this deferred initialization fails, all subsequent method
// calls should fail without side effects, return no data, and send no
// notifications. PasswordStoreSync is a hidden base class because only
// PasswordSyncableService needs to access these methods.
class PasswordStore : protected PasswordStoreSync,
                      public RefcountedKeyedService {
 public:
  // An interface used to notify clients (observers) of this object that data in
  // the password store has changed. Register the observer via
  // PasswordStore::AddObserver.
  class Observer {
   public:
    // Notifies the observer that password data changed. Will be called from
    // the UI thread.
    virtual void OnLoginsChanged(const PasswordStoreChangeList& changes) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Represents a subset of autofill::PasswordForm needed for credential
  // retrievals.
  struct FormDigest {
    FormDigest(autofill::PasswordForm::Scheme scheme,
               const std::string& signon_realm,
               const GURL& origin);
    explicit FormDigest(const autofill::PasswordForm& form);
    explicit FormDigest(const autofill::FormData& form);
    FormDigest(const FormDigest& other);
    FormDigest(FormDigest&& other);
    FormDigest& operator=(const FormDigest& other);
    FormDigest& operator=(FormDigest&& other);
    bool operator==(const FormDigest& other) const;

    autofill::PasswordForm::Scheme scheme;
    std::string signon_realm;
    GURL origin;
  };

  PasswordStore();

  // Always call this too on the UI thread.
  bool Init(
      const syncer::SyncableService::StartSyncFlare& flare,
      PrefService* prefs,
      base::RepeatingClosure sync_enabled_or_disabled_cb = base::DoNothing());

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // Sets the affiliation-based match |helper| that will be used by subsequent
  // GetLogins() calls to return credentials stored not only for the requested
  // sign-on realm, but also for affiliated Android applications. If |helper| is
  // null, clears the the currently set helper if any. Unless a helper is set,
  // affiliation-based matching is disabled. The passed |helper| must already be
  // initialized if it is non-null.
  void SetAffiliatedMatchHelper(std::unique_ptr<AffiliatedMatchHelper> helper);
  AffiliatedMatchHelper* affiliated_match_helper() const {
    return affiliated_match_helper_.get();
  }

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const autofill::PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  // If any of the primary key fields (signon_realm, origin, username_element,
  // username_value, password_element) are updated, then the second version of
  // the method must be used that takes |old_primary_key|, i.e., the old values
  // for the primary key fields (the rest of the fields are ignored).
  virtual void UpdateLogin(const autofill::PasswordForm& form);
  virtual void UpdateLoginWithPrimaryKey(
      const autofill::PasswordForm& new_form,
      const autofill::PasswordForm& old_primary_key);

  // Removes the matching PasswordForm from the secure password store (async).
  virtual void RemoveLogin(const autofill::PasswordForm& form);

  // Remove all logins whose origins match the given filter and that were
  // created
  // in the given date range. |completion| will be posted to the
  // |main_task_runner_| after deletions have been completed and notification
  // have been sent out.
  void RemoveLoginsByURLAndTime(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion);

  // Removes all logins created in the given date range. If |completion| is not
  // null, it will be posted to the |main_task_runner_| after deletions have
  // been completed and notification have been sent out.
  void RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end,
                                  const base::Closure& completion);

  // Removes all the stats created in the given date range.
  // If |origin_filter| is not null, only statistics for matching origins are
  // removed. If |completion| is not null, it will be posted to the
  // |main_task_runner_| after deletions have been completed.
  // Should be called on the UI thread.
  void RemoveStatisticsByOriginAndTime(
      const base::Callback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion);

  // Sets the 'skip_zero_click' flag for all logins in the database that match
  // |origin_filter| to 'true'. |completion| will be posted to the
  // |main_task_runner_| after these modifications are completed and
  // notifications are sent out.
  void DisableAutoSignInForOrigins(
      const base::Callback<bool(const GURL&)>& origin_filter,
      const base::Closure& completion);

  // Unblacklists the login with |form_digest| by deleting all the corresponding
  // blacklisted entries. If |completion| is not null, it will be posted to the
  // |main_task_runner_| after deletions have been completed. Should be called
  // on the UI thread.
  void Unblacklist(const PasswordStore::FormDigest& form_digest,
                   base::OnceClosure completion);

  // Searches for a matching PasswordForm, and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  virtual void GetLogins(const FormDigest& form,
                         PasswordStoreConsumer* consumer);

  // Searches for credentials with the specified |plain_text_password|, and
  // notifies |consumer| on completion. The request will be cancelled if the
  // consumer is destroyed.
  virtual void GetLoginsByPassword(const base::string16& plain_text_password,
                                   PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable. |consumer| will be notified on completion.
  // The request will be cancelled if the consumer is destroyed.
  virtual void GetAutofillableLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are blacklist entries and
  // notifies |consumer| on completion. The request will be cancelled if the
  // consumer is destroyed.
  virtual void GetBlacklistLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms (regardless of their blacklist
  // status) and notify |consumer| on completion. The request will be cancelled
  // if the consumer is destroyed.
  virtual void GetAllLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms, regardless of their blacklist
  // status. Also fills in affiliation and branding information for Android
  // credentials.
  virtual void GetAllLoginsWithAffiliationAndBrandingInformation(
      PasswordStoreConsumer* consumer);

  // Reports usage metrics for the database. |sync_username|, and
  // |custom_passphrase_sync_enabled|, and |is_under_advanced_protection|
  // determine some of the UMA stats that may be reported.
  virtual void ReportMetrics(const std::string& sync_username,
                             bool custom_passphrase_sync_enabled,
                             bool is_under_advanced_protection);

  // Adds or replaces the statistics for the domain |stats.origin_domain|.
  void AddSiteStats(const InteractionsStats& stats);

  // Removes the statistics for |origin_domain|.
  void RemoveSiteStats(const GURL& origin_domain);

  // Retrieves the statistics for |origin_domain| and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  void GetSiteStats(const GURL& origin_domain, PasswordStoreConsumer* consumer);

  // Adds information about credentials compromised on
  // |compromised_credentials.url| for |compromised_credentials.username|. The
  // first |compromised_credentials.create_time| is kept, so if the record for
  // given url and username already exists, the new one will be ignored.
  void AddCompromisedCredentials(
      const CompromisedCredentials& compromised_credentials);

  // Removes information about credentials compromised on |url| for |username|.
  void RemoveCompromisedCredentials(const GURL& url,
                                    const base::string16& username);

  // Retrieves all compromised credentials and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  void GetAllCompromisedCredentials(PasswordLeakHistoryConsumer* consumer);

  // Removes all compromised credentials in the given date range. If
  // |url_filter| is not null, only compromised credentials for matching urls
  // are removed. If |completion| is not null, it will be posted to the
  // |main_task_runner_| after deletions have been completed. Should be called
  // on the UI thread.
  void RemoveCompromisedCredentialsByUrlAndTime(
      base::RepeatingCallback<bool(const GURL&)> url_filter,
      base::Time remove_begin,
      base::Time remove_end,
      base::OnceClosure completion);

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

  // Adds an observer to be notified when the password store data changes.
  void AddObserver(Observer* observer);

  // Removes |observer| from the observer list.
  void RemoveObserver(Observer* observer);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(base::OnceClosure task);

  // Returns true iff initialization was successful.
  virtual bool IsAbleToSavePasswords() const;

  base::WeakPtr<syncer::SyncableService> GetPasswordSyncableService();

  // For sync codebase only: instantiates a proxy controller delegate to
  // interact with PasswordSyncBridge. Must be called from the UI thread.
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate();

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Immediately called after |Init()| to retrieve password hash data for
  // reuse detection.
  void PreparePasswordHashData(const std::string& sync_username);

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain|.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // sequence on which this method is called.
  // |consumer| must not be null.
  virtual void CheckReuse(const base::string16& input,
                          const std::string& domain,
                          PasswordReuseDetectorConsumer* consumer);

  // Saves |username| and a hash of |password| for GAIA password reuse checking.
  // |event| is used for metric logging and for distinguishing sync password
  // hash change event and other non-sync GAIA password change event.
  virtual void SaveGaiaPasswordHash(const std::string& username,
                                    const base::string16& password,
                                    GaiaPasswordHashChange event);

  // Saves |username| and a hash of |password| for enterprise password reuse
  // checking.
  virtual void SaveEnterprisePasswordHash(const std::string& username,
                                          const base::string16& password);

  // Saves |sync_password_data| for sync password reuse checking.
  // |event| is used for metric logging.
  virtual void SaveSyncPasswordHash(const PasswordHashData& sync_password_data,
                                    GaiaPasswordHashChange event);

  // Clears the saved GAIA password hash for |username|.
  virtual void ClearGaiaPasswordHash(const std::string& username);

  // Clears all the GAIA password hash.
  virtual void ClearAllGaiaPasswordHash();

  // Clears all (non-GAIA) enterprise password hash.
  virtual void ClearAllEnterprisePasswordHash();

  // Clear all GAIA password hash that is not associated with a Gmail account.
  virtual void ClearAllNonGmailPasswordHash();

  // Adds a listener on |hash_password_manager_| for when |kHashPasswordData|
  // list might have changed. Should only be called on the UI thread.
  virtual std::unique_ptr<StateSubscription>
  RegisterStateCallbackOnHashPasswordManager(
      const base::Callback<void(const std::string& username)>& callback);

  // Shouldn't be called more than once, |notifier| must be not nullptr.
  void SetPasswordStoreSigninNotifier(
      std::unique_ptr<PasswordStoreSigninNotifier> notifier);

  // Schedules the update of password hashes used by reuse detector.
  void SchedulePasswordHashUpdate(bool should_log_metrics);

  // Schedules the update of enterprise login and change password URLs.
  // These URLs are used in enterprise password reuse detection.
  void ScheduleEnterprisePasswordURLUpdate();

#endif

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Represents a single CheckReuse() request. Implements functionality to
  // listen to reuse events and propagate them to |consumer| on the sequence on
  // which CheckReuseRequest is created.
  class CheckReuseRequest : public PasswordReuseDetectorConsumer {
   public:
    // |consumer| must not be null.
    explicit CheckReuseRequest(PasswordReuseDetectorConsumer* consumer);
    ~CheckReuseRequest() override;

    // PasswordReuseDetectorConsumer
    void OnReuseFound(
        size_t password_length,
        base::Optional<PasswordHashData> reused_protected_password_hash,
        const std::vector<std::string>& matching_domains,
        int saved_passwords) override;

   private:
    const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
    const base::WeakPtr<PasswordReuseDetectorConsumer> consumer_weak_;

    DISALLOW_COPY_AND_ASSIGN(CheckReuseRequest);
  };
#endif

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

  // Create a TaskRunner to be saved in |background_task_runner_|.
  virtual scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const;

  // Creates PasswordSyncableService and PasswordReuseDetector instances on the
  // background sequence. Subclasses can add more logic. Returns true on
  // success.
  virtual bool InitOnBackgroundSequence(
      const syncer::SyncableService::StartSyncFlare& flare);

  // Methods below will be run in PasswordStore's own sequence.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username,
                                 bool custom_passphrase_sync_enabled) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the statistics.
  virtual bool RemoveStatisticsByOriginAndTimeImpl(
      const base::Callback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to disable auto sign-in.
  virtual PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::Callback<bool(const GURL&)>& origin_filter) = 0;

  // Synchronous implementation provided by subclasses to add the given login.
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form,
      AddLoginError* error = nullptr) = 0;

  // Synchronous implementation provided by subclasses to update the given
  // login.
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form,
      UpdateLoginError* error = nullptr) = 0;

  // Synchronous implementation provided by subclasses to remove the given
  // login.
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Finds and returns all PasswordForms with the same signon_realm as |form|,
  // or with a signon_realm that is a PSL-match to that of |form|.
  virtual std::vector<std::unique_ptr<autofill::PasswordForm>>
  FillMatchingLogins(const FormDigest& form) = 0;

  // Finds and returns all not-blacklisted PasswordForms with the specified
  // |plain_text_password| stored in the credential database.
  virtual std::vector<std::unique_ptr<autofill::PasswordForm>>
  FillMatchingLoginsByPassword(const base::string16& plain_text_password) = 0;

  // Synchronous implementation for manipulating with statistics.
  virtual void AddSiteStatsImpl(const InteractionsStats& stats) = 0;
  virtual void RemoveSiteStatsImpl(const GURL& origin_domain) = 0;
  virtual std::vector<InteractionsStats> GetAllSiteStatsImpl() = 0;
  virtual std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) = 0;

  // Synchronous implementation for manipulating with information about
  // compromised credentials.
  virtual void AddCompromisedCredentialsImpl(
      const CompromisedCredentials& compromised_credentials) = 0;
  // TODO(bdea): Add CompromiseType as a filter.
  virtual void RemoveCompromisedCredentialsImpl(
      const GURL& url,
      const base::string16& username) = 0;
  virtual std::vector<CompromisedCredentials>
  GetAllCompromisedCredentialsImpl() = 0;
  virtual void RemoveCompromisedCredentialsByUrlAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) = 0;

  // Synchronous implementation for manipulating with information about field
  // info.
  virtual void AddFieldInfoImpl(const FieldInfo& field_info) = 0;
  virtual std::vector<FieldInfo> GetAllFieldInfoImpl() = 0;
  virtual void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                         base::Time remove_end) = 0;

  // PasswordStoreSync:
  PasswordStoreChangeList AddLoginSync(const autofill::PasswordForm& form,
                                       AddLoginError* error) override;
  PasswordStoreChangeList UpdateLoginSync(const autofill::PasswordForm& form,
                                          UpdateLoginError* error) override;
  PasswordStoreChangeList RemoveLoginSync(
      const autofill::PasswordForm& form) override;

  // Called by *Internal() methods once the underlying data-modifying operation
  // has been performed. Notifies observers that password store data may have
  // been changed.
  void NotifyLoginsChanged(const PasswordStoreChangeList& changes) override;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Saves |username| and a hash of |password| for password reuse checking.
  // |is_gaia_password| indicates if it is a Gaia account. |event| is used for
  // metric logging.
  void SaveProtectedPasswordHash(const std::string& username,
                                 const base::string16& password,
                                 bool is_gaia_password,
                                 GaiaPasswordHashChange event);

  // Synchronous implementation of CheckReuse().
  void CheckReuseImpl(std::unique_ptr<CheckReuseRequest> request,
                      const base::string16& input,
                      const std::string& domain);

  // Synchronous implementation of SaveProtectedPasswordHash().
  // |should_log_metrics| indicates whether to log the counts of captured
  // password hashes.
  void SaveProtectedPasswordHashImpl(
      PasswordHashDataList protected_password_data_list,
      bool should_log_metrics);

  // Propagates enterprise login urls and change password url to
  // |reuse_detector_|.
  void SaveEnterprisePasswordURLs(
      const std::vector<GURL>& enterprise_login_urls,
      const GURL& enterprise_change_password_url);

  // Synchronous implementation of ClearGaiaPasswordHash(...).
  void ClearGaiaPasswordHashImpl(const std::string& username);

  // Synchronous implementation of ClearAllGaiaPasswordHash().
  void ClearAllGaiaPasswordHashImpl();

  // Synchronous implementation of ClearAllEnterprisePasswordHash().
  void ClearAllEnterprisePasswordHashImpl();

  // Synchronous implementation of ClearAllNonGmailPasswordHash().
  void ClearAllNonGmailPasswordHashImpl();
#endif

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() const {
    return main_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest,
                           UpdatePasswordsStoredForAffiliatedWebsites);

  using LoginsResult = std::vector<std::unique_ptr<autofill::PasswordForm>>;
  using LoginsTask = base::OnceCallback<LoginsResult()>;
  using LoginsReply = base::OnceCallback<void(LoginsResult)>;
  using LoginsResultProcessor =
      base::OnceCallback<void(LoginsReply, LoginsResult)>;

  using StatsResult = std::vector<InteractionsStats>;
  using StatsTask = base::OnceCallback<StatsResult()>;

  using CompromisedCredentialsResult = std::vector<CompromisedCredentials>;
  using CompromisedCredentialsTask =
      base::OnceCallback<CompromisedCredentialsResult()>;

  // Called on the main thread after initialization is completed.
  // |success| is true if initialization was successful. Sets the
  // |init_status_|.
  void OnInitCompleted(bool success);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetPasswordStoreResults() on the caller's thread with
  // the result.
  void PostLoginsTaskAndReplyToConsumerWithResult(
      PasswordStoreConsumer* consumer,
      LoginsTask task);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  // Invokes |consumer|->OnGetPasswordStoreResults() on the caller's thread with
  // the result, after it was post-processed by |processor|.
  void PostLoginsTaskAndReplyToConsumerWithProcessedResult(
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
  // Invokes |consumer|->OnGetCompromisedCredentials() on the caller's thread
  // with the result.
  void PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
      PasswordLeakHistoryConsumer* consumer,
      CompromisedCredentialsTask task);

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChanged(). Note that there is no guarantee
  // that the called method will actually modify the password store data.
  void AddLoginInternal(const autofill::PasswordForm& form);
  void UpdateLoginInternal(const autofill::PasswordForm& form);
  void RemoveLoginInternal(const autofill::PasswordForm& form);
  void UpdateLoginWithPrimaryKeyInternal(
      const autofill::PasswordForm& new_form,
      const autofill::PasswordForm& old_primary_key);
  void RemoveLoginsByURLAndTimeInternal(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion);
  void RemoveLoginsCreatedBetweenInternal(base::Time delete_begin,
                                          base::Time delete_end,
                                          const base::Closure& completion);
  void RemoveStatisticsByOriginAndTimeInternal(
      const base::Callback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion);
  void DisableAutoSignInForOriginsInternal(
      const base::Callback<bool(const GURL&)>& origin_filter,
      const base::Closure& completion);
  void UnblacklistInternal(const PasswordStore::FormDigest& form_digest,
                           base::OnceClosure completion);
  void RemoveCompromisedCredentialsByUrlAndTimeInternal(
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
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetLoginsImpl(
      const FormDigest& form);

  // Finds all credentials with the specified |plain_text_password|.
  // Note: subclasses should implement FillMatchingLoginsByPassword() instead.
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetLoginsByPasswordImpl(
      const base::string16& plain_text_password);

  // Finds all non-blacklist PasswordForms and returns the result.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetAutofillableLoginsImpl();

  // Finds all blacklist PasswordForms and returns the result.
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetBlacklistLoginsImpl();

  // Finds all PasswordForms and returns the result.
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllLoginsImpl();

  // Extended version of GetLoginsImpl that also returns credentials stored for
  // the specified affiliated Android applications. That is, it finds all
  // PasswordForms with a signon_realm that is either:
  //  * equal to that of |form|,
  //  * is a PSL-match to the realm of |form|,
  //  * is one of those in |additional_android_realms|,
  // and returns the result.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetLoginsWithAffiliationsImpl(
      const FormDigest& form,
      const std::vector<std::string>& additional_android_realms);

  // Retrieves and fills in affiliation and branding information for Android
  // credentials in |forms| and invokes |callback| with the result. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(LoginsReply callback,
                                               LoginsResult forms);

  // Schedules GetLoginsWithAffiliationsImpl() to be run on the background
  // sequence. Logins older than |cutoff| will be deleted before |consumer| is
  // notified with the result.
  void ScheduleGetFilteredLoginsWithAffiliations(
      base::WeakPtr<PasswordStoreConsumer> consumer,
      const PasswordStore::FormDigest& form,
      base::Time cutoff,
      const std::vector<std::string>& additional_android_realms);

  // Retrieves the currently stored form, if any, with the same primary key as
  // |form|, that is, with the same signon_realm, origin, username_element,
  // username_value and password_element attributes. To be called on the
  // background sequence.
  std::unique_ptr<autofill::PasswordForm> GetLoginImpl(
      const autofill::PasswordForm& primary_key);

  // Called when a password is added or updated for an Android application, and
  // triggers finding web sites affiliated with the Android application and
  // propagating the new password to credentials for those web sites, if any.
  // Called on the main sequence.
  void FindAndUpdateAffiliatedWebLogins(
      const autofill::PasswordForm& added_or_updated_android_form);

  // Posts FindAndUpdateAffiliatedWebLogins() to the main sequence. Should be
  // called from the background sequence.
  void ScheduleFindAndUpdateAffiliatedWebLogins(
      const autofill::PasswordForm& added_or_updated_android_form);

  // Called when a password is added or updated for an Android application, and
  // propagates these changes to credentials stored for |affiliated_web_realms|
  // under the same username, if there are any. Called on the background
  // sequence.
  void UpdateAffiliatedWebLoginsImpl(
      const autofill::PasswordForm& updated_android_form,
      const std::vector<std::string>& affiliated_web_realms);

  // Returns the sync controller delegate for syncing passwords. It must be
  // called on the background sequence.
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence();

  // Schedules UpdateAffiliatedWebLoginsImpl() to run on the background
  // sequence. Should be called from the main sequence.
  void ScheduleUpdateAffiliatedWebLoginsImpl(
      const autofill::PasswordForm& updated_android_form,
      const std::vector<std::string>& affiliated_web_realms);

  // Deletes object that should be destroyed on the background sequence.
  // WARNING: this method can be skipped on shutdown.
  void DestroyOnBackgroundSequence();

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The observers.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // Either of two below would actually be set based on a feature flag.
  std::unique_ptr<PasswordSyncableService> syncable_service_;
  std::unique_ptr<PasswordSyncBridge> sync_bridge_;

  base::RepeatingClosure sync_enabled_or_disabled_cb_;

  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  PrefService* prefs_ = nullptr;
  // PasswordReuseDetector can be only destroyed on the background sequence. It
  // can't be owned by PasswordStore because PasswordStore can be destroyed on
  // the UI thread and DestroyOnBackgroundThread isn't guaranteed to be called.
  PasswordReuseDetector* reuse_detector_ = nullptr;
  std::unique_ptr<PasswordStoreSigninNotifier> notifier_;
  HashPasswordManager hash_password_manager_;
#endif

  bool shutdown_called_;

  InitStatus init_status_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

// For logging only.
std::ostream& operator<<(std::ostream& os,
                         const PasswordStore::FormDigest& digest);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
