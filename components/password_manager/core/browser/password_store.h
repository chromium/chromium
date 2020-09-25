// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/util/type_safety/strong_alias.h"
#include "build/build_config.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#endif

class PrefService;

namespace autofill {
struct FormData;
}  // namespace autofill

namespace syncer {
class ModelTypeControllerDelegate;
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

using StateSubscription =
    base::CallbackList<void(const std::string& username)>::Subscription;

namespace password_manager {

using IsAccountStore = util::StrongAlias<class IsAccountStoreTag, bool>;

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
using metrics_util::GaiaPasswordHashChange;
#endif

class AffiliatedMatchHelper;
class PasswordStoreConsumer;
class CompromisedCredentialsConsumer;
class PasswordStoreSigninNotifier;
class PasswordSyncBridge;
struct FieldInfo;
struct InteractionsStats;
struct CompromisedCredentials;

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
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
// PasswordSyncBridge needs to access these methods.
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

    // Like OnLoginsChanged(), but also receives the originating PasswordStore
    // as a parameter. This is useful for observers that observe changes in both
    // the profile-scoped and the account-scoped store. The default
    // implementation simply calls OnLoginsChanged(), so observers that don't
    // care about the store can just ignore this.
    virtual void OnLoginsChangedIn(PasswordStore* store,
                                   const PasswordStoreChangeList& changes);

   protected:
    virtual ~Observer() = default;
  };

  class DatabaseCompromisedCredentialsObserver {
    // An interface used to notify clients (observers) of this object that the
    // list of compromised credentials in the password store has changed.
    // Register the observer via
    // PasswordStore::AddDatabaseCompromisedCredentialsObserver.
   public:
    // Notifies the observer that the list of compromised credentials changed.
    // Will be called from the UI thread.
    virtual void OnCompromisedCredentialsChanged() = 0;

    // Like OnCompromisedCredentialsChanged(), but also receives the originating
    // PasswordStore as a parameter. This is useful for observers that observe
    // changes in both the profile-scoped and the account-scoped store. The
    // default implementation simply calls OnCompromisedCredentialsChanged(), so
    // observers that don't care about the store can just ignore this.
    virtual void OnCompromisedCredentialsChangedIn(PasswordStore* store);

   protected:
    virtual ~DatabaseCompromisedCredentialsObserver() = default;
  };

  // Used to notify that unsynced credentials are about to be deleted.
  class UnsyncedCredentialsDeletionNotifier {
   public:
    // Should be called from the UI thread.
    virtual void Notify(std::vector<PasswordForm>) = 0;
    virtual ~UnsyncedCredentialsDeletionNotifier() = default;
    virtual base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() = 0;
  };

  // Represents a subset of PasswordForm needed for credential
  // retrievals.
  struct FormDigest {
    FormDigest(PasswordForm::Scheme scheme,
               const std::string& signon_realm,
               const GURL& url);
    explicit FormDigest(const PasswordForm& form);
    explicit FormDigest(const autofill::FormData& form);
    FormDigest(const FormDigest& other);
    FormDigest(FormDigest&& other);
    FormDigest& operator=(const FormDigest& other);
    FormDigest& operator=(FormDigest&& other);
    bool operator==(const FormDigest& other) const;
    bool operator!=(const FormDigest& other) const;

    PasswordForm::Scheme scheme;
    std::string signon_realm;
    GURL url;
  };

  PasswordStore();

  // Always call this too on the UI thread.
  bool Init(
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
  virtual void AddLogin(const PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  // If any of the primary key fields (signon_realm, url, username_element,
  // username_value, password_element) are updated, then the second version of
  // the method must be used that takes |old_primary_key|, i.e., the old values
  // for the primary key fields (the rest of the fields are ignored).
  virtual void UpdateLogin(const PasswordForm& form);
  virtual void UpdateLoginWithPrimaryKey(const PasswordForm& new_form,
                                         const PasswordForm& old_primary_key);

  // Removes the matching PasswordForm from the secure password store (async).
  virtual void RemoveLogin(const PasswordForm& form);

  // Remove all logins whose origins match the given filter and that were
  // created in the given date range. |completion| will be posted to the
  // |main_task_runner_| after deletions have been completed and notifications
  // have been sent out. |sync_completion| will be posted to
  // |main_task_runner_| once the deletions have also been propagated to the
  // server (or, in rare cases, if the user permanently disables Sync or
  // deletions haven't been propagated after 30 seconds). This is
  // only relevant for Sync users and for account store users - for other users,
  // |sync_completion| will be run immediately after |completion|.
  void RemoveLoginsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion,
      base::OnceCallback<void(bool)> sync_completion = base::NullCallback());

  // Removes all logins created in the given date range. If |completion| is not
  // null, it will be posted to the |main_task_runner_| after deletions have
  // been completed and notification have been sent out.
  void RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end,
                                  base::OnceClosure completion);

  // Removes all the stats created in the given date range.
  // If |origin_filter| is not null, only statistics for matching origins are
  // removed. If |completion| is not null, it will be posted to the
  // |main_task_runner_| after deletions have been completed.
  // Should be called on the UI thread.
  void RemoveStatisticsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion);

  // Sets the 'skip_zero_click' flag for all logins in the database that match
  // |origin_filter| to 'true'. |completion| will be posted to the
  // |main_task_runner_| after these modifications are completed and
  // notifications are sent out.
  void DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion);

  // Unblacklists the login with |form_digest| by deleting all the corresponding
  // blacklisted entries. If |completion| is not null, it will be posted to the
  // |main_task_runner_| after deletions have been completed. Should be called
  // on the UI thread.
  virtual void Unblacklist(const PasswordStore::FormDigest& form_digest,
                           base::OnceClosure completion);

  // Searches for a matching PasswordForm, and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  virtual void GetLogins(const FormDigest& form,
                         PasswordStoreConsumer* consumer);

  // Searches for credentials with the specified |plain_text_password|, and
  // notifies |consumer| on completion. The request will be cancelled if the
  // consumer is destroyed.
  void GetLoginsByPassword(const base::string16& plain_text_password,
                           PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable. |consumer| will be notified on completion.
  // The request will be cancelled if the consumer is destroyed.
  virtual void GetAutofillableLogins(PasswordStoreConsumer* consumer);

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

  // TODO(crbug/1081389): replace GURL with Origin.
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

  // Removes information about credentials compromised on |signon_realm| for
  // |username|.
  void RemoveCompromisedCredentials(const std::string& signon_realm,
                                    const base::string16& username,
                                    RemoveCompromisedCredentialsReason reason);

  // Removes information about credentials compromised on |signon_realm| for
  // |username| and |compromise_type|.
  void RemoveCompromisedCredentialsByCompromiseType(
      const std::string& signon_realm,
      const base::string16& username,
      const CompromiseType& compromise_type,
      RemoveCompromisedCredentialsReason reason);

  // Retrieves all compromised credentials and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  void GetAllCompromisedCredentials(CompromisedCredentialsConsumer* consumer);

  // Returns all the compromised records for a given site. This list also
  // includes Android affiliated credentials.
  void GetMatchingCompromisedCredentials(
      const std::string& signon_realm,
      CompromisedCredentialsConsumer* consumer);

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

  // Deletes and re-creates the whole PasswordStore, unless it is already empty
  // anyway. If |completion| is not null, it will be posted to the
  // |main_task_runner_| once the process is complete. The bool parameter
  // indicates whether any data was actually cleared.
  void ClearStore(base::OnceCallback<void(bool)> completion);

  // Adds an observer to be notified when the password store data changes.
  void AddObserver(Observer* observer);

  // Removes |observer| from the observer list.
  void RemoveObserver(Observer* observer);

  // Adds an observer to be notified when the list of compromised passwords in
  // the password store changes.
  void AddDatabaseCompromisedCredentialsObserver(
      DatabaseCompromisedCredentialsObserver* observer);

  // Removes |observer| from the list of compromised credentials observer.
  void RemoveDatabaseCompromisedCredentialsObserver(
      DatabaseCompromisedCredentialsObserver* observer);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(base::OnceClosure task);

  // Returns true iff initialization was successful.
  virtual bool IsAbleToSavePasswords() const;

  // For sync codebase only: instantiates a proxy controller delegate to
  // interact with PasswordSyncBridge. Must be called from the UI thread.
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate();

  // Sets |deletion_notifier_|. Must not pass a nullptr.
  void SetUnsyncedCredentialsDeletionNotifier(
      std::unique_ptr<UnsyncedCredentialsDeletionNotifier> deletion_notifier);

  void SetSyncTaskTimeoutForTest(base::TimeDelta timeout);

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // Immediately called after |Init()| to retrieve password hash data for
  // reuse detection.
  void PreparePasswordHashData(const std::string& sync_username,
                               bool is_signed_in);

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
  // |is_primary_account| is whether account belong to the password is a
  // primary account.
  virtual void SaveGaiaPasswordHash(const std::string& username,
                                    const base::string16& password,
                                    bool is_primary_account,
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
      const base::RepeatingCallback<void(const std::string& username)>&
          callback);

  // Shouldn't be called more than once, |notifier| must be not nullptr.
  void SetPasswordStoreSigninNotifier(
      std::unique_ptr<PasswordStoreSigninNotifier> notifier);

  // Schedules the update of password hashes used by reuse detector.
  // |does_primary_account_exists| and |is_signed_in| fields are only used if
  // |should_log_metrics| is true.
  void SchedulePasswordHashUpdate(bool should_log_metrics,
                                  bool does_primary_account_exists,
                                  bool is_signed_in);

  // Schedules the update of enterprise login and change password URLs.
  // These URLs are used in enterprise password reuse detection.
  void ScheduleEnterprisePasswordURLUpdate();

#endif

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // Represents a single CheckReuse() request. Implements functionality to
  // listen to reuse events and propagate them to |consumer| on the sequence on
  // which CheckReuseRequest is created.
  class CheckReuseRequest : public PasswordReuseDetectorConsumer {
   public:
    // |consumer| must not be null.
    explicit CheckReuseRequest(PasswordReuseDetectorConsumer* consumer);
    ~CheckReuseRequest() override;

    // PasswordReuseDetectorConsumer
    void OnReuseCheckDone(
        bool is_reuse_found,
        size_t password_length,
        base::Optional<PasswordHashData> reused_protected_password_hash,
        const std::vector<MatchingReusedCredential>&
            matching_reused_credentials,
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

  // Creates PasswordSyncBridge and PasswordReuseDetector instances on the
  // background sequence. Subclasses can add more logic. Returns true on
  // success.
  virtual bool InitOnBackgroundSequence();

  // Methods below will be run in PasswordStore's own sequence.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username,
                                 bool custom_passphrase_sync_enabled,
                                 BulkCheckDone bulk_check_done) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the statistics.
  virtual bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to disable auto sign-in.
  virtual PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) = 0;

  // Synchronous implementation provided by subclasses to add the given login.
  virtual PasswordStoreChangeList AddLoginImpl(
      const PasswordForm& form,
      AddLoginError* error = nullptr) = 0;

  // Synchronous implementation provided by subclasses to update the given
  // login.
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const PasswordForm& form,
      UpdateLoginError* error = nullptr) = 0;

  // Synchronous implementation provided by subclasses to remove the given
  // login.
  virtual PasswordStoreChangeList RemoveLoginImpl(const PasswordForm& form) = 0;

  // Finds and returns all PasswordForms with the same signon_realm as |form|,
  // or with a signon_realm that is a PSL-match to that of |form|.
  virtual std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const FormDigest& form) = 0;

  // Finds and returns all not-blacklisted PasswordForms with the specified
  // |plain_text_password| stored in the credential database.
  virtual std::vector<std::unique_ptr<PasswordForm>>
  FillMatchingLoginsByPassword(const base::string16& plain_text_password) = 0;

  // Overwrites |forms| with all stored non-blacklisted credentials. Returns
  // true on success.
  virtual bool FillAutofillableLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) WARN_UNUSED_RESULT = 0;

  // Overwrites |forms| with all stored blacklisted credentials. Returns true on
  // success.
  virtual bool FillBlacklistLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) WARN_UNUSED_RESULT = 0;

  // Synchronous implementation for manipulating with statistics.
  virtual void AddSiteStatsImpl(const InteractionsStats& stats) = 0;
  virtual void RemoveSiteStatsImpl(const GURL& origin_domain) = 0;
  virtual std::vector<InteractionsStats> GetAllSiteStatsImpl() = 0;
  virtual std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) = 0;

  // Synchronous implementation for manipulating with information about
  // compromised credentials.
  virtual bool AddCompromisedCredentialsImpl(
      const CompromisedCredentials& compromised_credentials) = 0;
  virtual bool RemoveCompromisedCredentialsImpl(
      const std::string& signon_realm,
      const base::string16& username,
      RemoveCompromisedCredentialsReason reason) = 0;
  virtual bool RemoveCompromisedCredentialsByCompromiseTypeImpl(
      const std::string& signon_realm,
      const base::string16& username,
      const CompromiseType& compromised_type,
      RemoveCompromisedCredentialsReason reason) = 0;
  virtual std::vector<CompromisedCredentials>
  GetAllCompromisedCredentialsImpl() = 0;
  virtual std::vector<CompromisedCredentials>
  GetMatchingCompromisedCredentialsImpl(const std::string& signon_realm) = 0;
  virtual bool RemoveCompromisedCredentialsByUrlAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) = 0;

  // Synchronous implementation for manipulating with information about field
  // info.
  virtual void AddFieldInfoImpl(const FieldInfo& field_info) = 0;
  virtual std::vector<FieldInfo> GetAllFieldInfoImpl() = 0;
  virtual void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                         base::Time remove_end) = 0;

  // Synchronous implementation provided by subclasses to check whether the
  // store is empty.
  virtual bool IsEmpty() = 0;

  // PasswordStoreSync:
  PasswordStoreChangeList AddLoginSync(const PasswordForm& form,
                                       AddLoginError* error) override;
  PasswordStoreChangeList UpdateLoginSync(const PasswordForm& form,
                                          UpdateLoginError* error) override;
  PasswordStoreChangeList RemoveLoginSync(const PasswordForm& form) override;

  // Called by *Internal() methods once the underlying data-modifying operation
  // has been performed. Notifies observers that password store data may have
  // been changed.
  void NotifyLoginsChanged(const PasswordStoreChangeList& changes) override;

  void NotifyDeletionsHaveSynced(bool success) override;

  void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<PasswordForm> unsynced_credentials) override;

  // Invokes callback and notifies observers if there was a change to the list
  // of compromised passwords.
  void InvokeAndNotifyAboutCompromisedPasswordsChange(
      base::OnceCallback<bool()> callback);

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // Saves |username| and a hash of |password| for password reuse checking.
  // |is_gaia_password| indicates if it is a Gaia account. |event| is used for
  // metric logging. |is_primary_account| is whether account belong to the
  // password is a primary account.
  void SaveProtectedPasswordHash(const std::string& username,
                                 const base::string16& password,
                                 bool is_primary_account,
                                 bool is_gaia_password,
                                 GaiaPasswordHashChange event);

  // Synchronous implementation of CheckReuse().
  void CheckReuseImpl(std::unique_ptr<CheckReuseRequest> request,
                      const base::string16& input,
                      const std::string& domain);

  // Synchronous implementation of SaveProtectedPasswordHash().
  // |should_log_metrics| indicates whether to log the counts of captured
  // password hashes. |does_primary_account_exists| is used to differentiate
  // between the metrics.
  void SaveProtectedPasswordHashImpl(
      PasswordHashDataList protected_password_data_list,
      bool should_log_metrics,
      bool does_primary_account_exists,
      bool is_signed_in);

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

  using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
  using LoginsTask = base::OnceCallback<LoginsResult()>;
  using LoginsReply = base::OnceCallback<void(LoginsResult)>;
  using LoginsResultProcessor =
      base::OnceCallback<void(LoginsReply, LoginsResult)>;

  using StatsResult = std::vector<InteractionsStats>;
  using StatsTask = base::OnceCallback<StatsResult()>;

  using CompromisedCredentialsTask =
      base::OnceCallback<std::vector<CompromisedCredentials>()>;

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
  // Invokes |consumer|->OnGetCompromisedCredentials() on the caller's thread
  // with the result.
  void PostCompromisedCredentialsTaskAndReplyToConsumerWithResult(
      CompromisedCredentialsConsumer* consumer,
      CompromisedCredentialsTask task);

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChanged(). Note that there is no guarantee
  // that the called method will actually modify the password store data.
  void AddLoginInternal(const PasswordForm& form);
  void UpdateLoginInternal(const PasswordForm& form);
  void RemoveLoginInternal(const PasswordForm& form);
  void UpdateLoginWithPrimaryKeyInternal(const PasswordForm& new_form,
                                         const PasswordForm& old_primary_key);
  void RemoveLoginsByURLAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion,
      base::OnceCallback<void(bool)> sync_completion);
  void RemoveLoginsCreatedBetweenInternal(base::Time delete_begin,
                                          base::Time delete_end,
                                          base::OnceClosure completion);
  void RemoveStatisticsByOriginAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion);
  void DisableAutoSignInForOriginsInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion);
  void UnblacklistInternal(const PasswordStore::FormDigest& form_digest,
                           base::OnceClosure completion);
  bool RemoveCompromisedCredentialsByUrlAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end,
      base::OnceClosure completion);

  void RemoveFieldInfoByTimeInternal(base::Time remove_begin,
                                     base::Time remove_end,
                                     base::OnceClosure completion);

  void ClearStoreInternal(base::OnceCallback<void(bool)> completion);

  // Finds all PasswordForms with a signon_realm that is equal to, or is a
  // PSL-match to that of |form|, and takes care of notifying the consumer with
  // the results when done.
  // Note: subclasses should implement FillMatchingLogins() instead.
  std::vector<std::unique_ptr<PasswordForm>> GetLoginsImpl(
      const FormDigest& form);

  // Finds all credentials with the specified |plain_text_password|.
  // Note: subclasses should implement FillMatchingLoginsByPassword() instead.
  std::vector<std::unique_ptr<PasswordForm>> GetLoginsByPasswordImpl(
      const base::string16& plain_text_password);

  // Finds all non-blacklist PasswordForms and returns the result.
  std::vector<std::unique_ptr<PasswordForm>> GetAutofillableLoginsImpl();

  // Finds all blacklist PasswordForms and returns the result.
  std::vector<std::unique_ptr<PasswordForm>> GetBlacklistLoginsImpl();

  // Finds all PasswordForms and returns the result.
  std::vector<std::unique_ptr<PasswordForm>> GetAllLoginsImpl();

  // Extended version of GetLoginsImpl that also returns credentials stored for
  // the specified affiliated Android applications. That is, it finds all
  // PasswordForms with a signon_realm that is either:
  //  * equal to that of |form|,
  //  * is a PSL-match to the realm of |form|,
  //  * is one of those in |additional_android_realms|,
  // and returns the result.
  std::vector<std::unique_ptr<PasswordForm>> GetLoginsWithAffiliationsImpl(
      const FormDigest& form,
      const std::vector<std::string>& additional_android_realms);

  // Extended version of GetMatchingCompromisedCredentialsImpl that also returns
  // credentials stored for the specified affiliated Android applications.
  std::vector<CompromisedCredentials> GetCompromisedWithAffiliationsImpl(
      const std::string& signon_realm,
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

  // Schedules GetCompromisedWithAffiliationsImpl() to be run on the background
  // sequence.
  void ScheduleGetCompromisedWithAffiliations(
      base::WeakPtr<CompromisedCredentialsConsumer> consumer,
      const std::string& signon_realm,
      const std::vector<std::string>& additional_android_realms);

  // Retrieves the currently stored form, if any, with the same primary key as
  // |form|, that is, with the same signon_realm, url, username_element,
  // username_value and password_element attributes. To be called on the
  // background sequence.
  std::unique_ptr<PasswordForm> GetLoginImpl(const PasswordForm& primary_key);

  // Called when a password is added or updated for an Android application, and
  // triggers finding web sites affiliated with the Android application and
  // propagating the new password to credentials for those web sites, if any.
  // Called on the main sequence.
  void FindAndUpdateAffiliatedWebLogins(
      const PasswordForm& added_or_updated_android_form);

  // Posts FindAndUpdateAffiliatedWebLogins() to the main sequence. Should be
  // called from the background sequence.
  void ScheduleFindAndUpdateAffiliatedWebLogins(
      const PasswordForm& added_or_updated_android_form);

  // Called when a password is added or updated for an Android application, and
  // propagates these changes to credentials stored for |affiliated_web_realms|
  // under the same username, if there are any. Called on the background
  // sequence.
  void UpdateAffiliatedWebLoginsImpl(
      const PasswordForm& updated_android_form,
      const std::vector<std::string>& affiliated_web_realms);

  // Returns the sync controller delegate for syncing passwords. It must be
  // called on the background sequence.
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence();

  // Schedules UpdateAffiliatedWebLoginsImpl() to run on the background
  // sequence. Should be called from the main sequence.
  void ScheduleUpdateAffiliatedWebLoginsImpl(
      const PasswordForm& updated_android_form,
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
  scoped_refptr<
      base::ObserverListThreadSafe<DatabaseCompromisedCredentialsObserver>>
      compromised_credentials_observers_ =
          base::MakeRefCounted<base::ObserverListThreadSafe<
              DatabaseCompromisedCredentialsObserver>>();

  std::unique_ptr<PasswordSyncBridge> sync_bridge_;

  base::RepeatingClosure sync_enabled_or_disabled_cb_;

  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

  PrefService* prefs_ = nullptr;
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // PasswordReuseDetector can be only destroyed on the background sequence. It
  // can't be owned by PasswordStore because PasswordStore can be destroyed on
  // the UI thread and DestroyOnBackgroundThread isn't guaranteed to be called.
  PasswordReuseDetector* reuse_detector_ = nullptr;
  std::unique_ptr<PasswordStoreSigninNotifier> notifier_;
  HashPasswordManager hash_password_manager_;
#endif

  std::unique_ptr<UnsyncedCredentialsDeletionNotifier> deletion_notifier_;

  // A list of callbacks that should be run once all pending deletions have been
  // sent to the Sync server. Note that the vector itself lives on the
  // background thread, but the callbacks must be run on the main thread!
  std::vector<base::OnceCallback<void(bool)>> deletions_have_synced_callbacks_;
  // Timeout closure that runs if sync takes too long to propagate deletions.
  base::CancelableClosure deletions_have_synced_timeout_;

  bool shutdown_called_ = false;

  InitStatus init_status_ = InitStatus::kUnknown;

  // This is usually constant, only changed in tests.
  base::TimeDelta sync_task_timeout_ = base::TimeDelta::FromSeconds(30);

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

// For logging only.
std::ostream& operator<<(std::ostream& os,
                         const PasswordStore::FormDigest& digest);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
