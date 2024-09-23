// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/account_reconcilor_throttler.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/account_manager_facade.h"
#endif

class PrefRegistrySimple;

namespace signin {
class AccountReconcilorDelegate;
enum class SetAccountsInCookieResult;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class ConsistencyCookieManager;
class ConsistencyCookieManagerTest;
#endif
}  // namespace signin

class SigninClient;
struct CoreAccountId;

class AccountReconcilor
    : public KeyedService,
#if BUILDFLAG(IS_CHROMEOS)
      public account_manager::AccountManagerFacade::Observer,
#endif
      public content_settings::Observer,
      public signin::IdentityManager::Observer {
 public:
  // When an instance of this class exists, the account reconcilor is suspended.
  // It will automatically restart when all instances of Lock have been
  // destroyed.
  class Lock final {
   public:
    explicit Lock(AccountReconcilor* reconcilor);

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

    ~Lock();

   private:
    base::WeakPtr<AccountReconcilor> reconcilor_;
    THREAD_CHECKER(thread_checker_);
  };

  // Helper class to indicate that synced data is being deleted. The object
  // must be destroyed when the data deletion is complete.
  class ScopedSyncedDataDeletion {
   public:
    ScopedSyncedDataDeletion(const ScopedSyncedDataDeletion&) = delete;
    ScopedSyncedDataDeletion& operator=(const ScopedSyncedDataDeletion&) =
        delete;

    ~ScopedSyncedDataDeletion();

   private:
    friend class AccountReconcilor;
    explicit ScopedSyncedDataDeletion(AccountReconcilor* reconcilor);
    base::WeakPtr<AccountReconcilor> reconcilor_;
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // The typical order of events is:
    // - When reconcile is blocked:
    //   1. current reconcile is aborted with AbortReconcile(),
    //   2. OnStateChanged() is called with kScheduled.
    //   3. OnBlockReconcile() is called.
    // - When reconcile is unblocked:
    //   1. OnUnblockReconcile() is called,
    //   2. reconcile is restarted if needed with StartReconcile(), which
    //     triggers a call to OnStateChanged() with kRunning.

    // Called whe reconcile starts.
    virtual void OnStateChanged(signin_metrics::AccountReconcilorState state) {}
    // Called when the AccountReconcilor is blocked.
    virtual void OnBlockReconcile() {}
    // Called when the AccountReconcilor is unblocked.
    virtual void OnUnblockReconcile() {}
  };

#if BUILDFLAG(IS_CHROMEOS)
  AccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      account_manager::AccountManagerFacade* account_manager_facade,
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);
#else
  AccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);
#endif  // BUILDFLAG(IS_CHROMEOS)

  AccountReconcilor(const AccountReconcilor&) = delete;
  AccountReconcilor& operator=(const AccountReconcilor&) = delete;

  ~AccountReconcilor() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Initializes the account reconcilor. Should be called once after
  // construction.
  void Initialize(bool start_reconcile_if_tokens_available);

  // Enables and disables the reconciliation.
  void EnableReconcile();
  void DisableReconcile(bool logout_all_gaia_accounts);

  // Signal that an X-Chrome-Manage-Accounts was received from GAIA. Pass the
  // ServiceType specified by GAIA in the 204 response.
  // Virtual for testing.
  virtual void OnReceivedManageAccountsResponse(
      signin::GAIAServiceType service_type);

  // KeyedService implementation.
  void Shutdown() override;

  // Determine what the reconcilor is currently doing.
  signin_metrics::AccountReconcilorState GetState() const;

  // Adds ands removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ScopedSyncedDataDeletion can be created when synced data is being removed
  // and destroyed when the deletion is complete. It prevents the Sync account
  // from being invalidated during the deletion.
  std::unique_ptr<ScopedSyncedDataDeletion> GetScopedSyncDataDeletion();

  // Returns true if reconcilor is blocked.
  bool IsReconcileBlocked() const;

  // Returns the 'most severe' error encountered during the last attempt to
  // reconcile (after the state is already set to kOk or kError).
  // If the last reconciliation attempt was successful, this will be
  // `GoogleServiceAuthError::State::NONE`.
  GoogleServiceAuthError GetReconcileError() const;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Gets the ConsistencyCookieManager, which updates the
  // "CHROME_ID_CONSISTENCY_STATE" cookie.
  signin::ConsistencyCookieManager* GetConsistencyCookieManager();
#endif

 protected:
  void OnSetAccountsInCookieCompleted(
      const std::vector<CoreAccountId>& accounts_to_send,
      signin::SetAccountsInCookieResult result);
  void OnLogOutFromCookieCompleted(const GoogleServiceAuthError& error);

 private:
  friend class AccountReconcilorTest;
  friend class AccountReconcilorTestForceDiceMigration;
  friend class AccountReconcilorThrottlerTest;
  friend class BaseAccountReconcilorTestTable;
  friend class DiceBrowserTest;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  friend class signin::ConsistencyCookieManagerTest;
#endif

#if BUILDFLAG(ENABLE_MIRROR)
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ForceReconcileEarlyExitsForInactiveReconcilor);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           ForceReconcileImmediatelyStartsForIdleReconcilor);
  FRIEND_TEST_ALL_PREFIXES(
      AccountReconcilorMirrorTest,
      ForceReconcileImmediatelyStartsForErroredOutReconcilor);
  FRIEND_TEST_ALL_PREFIXES(
      AccountReconcilorMirrorTest,
      ForceReconcileSchedulesReconciliationIfReconcilorIsAlreadyRunning);
  FRIEND_TEST_ALL_PREFIXES(
      AccountReconcilorMirrorTest,
      OnSigninDialogClosedNotificationTriggersForcedReconciliation);
#endif  // BUILDFLAG(ENABLE_MIRROR)

  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestForceDiceMigration,
                           TableRowTestCheckNoOp);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           IdentityManagerRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, Reauth);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           ProfileAlreadyConnected);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestTable, TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestTable,
                           InconsistencyReasonLogging);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestDiceMultilogin, TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestMirrorMultilogin, TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestMiceMultilogin, TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMiceTest,
                           AccountReconcilorStateScheduled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           ClearPrimaryAccountNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           DiceTokenServiceRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           DiceReconcileWithoutSignin);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest, DiceReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           DiceLastKnownFirstAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest, UnverifiedAccountNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest, UnverifiedAccountMerge);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           HandleSigninDuringReconcile);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest,
                           DiceReconcileReuseGaiaFirstAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTest, DeleteCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTestForSupervisedUsers,
                           DeleteCookieForNonSyncingSupervisedUsers);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTestForSupervisedUsers,
                           DeleteCookieForSyncingSupervisedUsers);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTestWithUnoDesktop,
                           DeleteCookieForSignedInUser);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTestWithUnoDesktop,
                           DeleteCookieForSyncingUser);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorDiceTestWithUnoDesktop,
                           PendingStateThenClearPrimaryAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, TokensNotLoaded);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileCookiesDisabled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileContentSettings);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileContentSettingsGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileContentSettingsNonGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileContentSettingsWildcardPattern);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           GetAccountsFromCookieSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           EnableReconcileWhileAlreadyRunning);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           GetAccountsFromCookieFailure);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           ExtraCookieChangeNotification);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, StartReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileNoopWithDots);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileNoopMultiple);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileAddToCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, AuthErrorTriggersListAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           SignoutAfterErrorDoesNotRecordUma);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, TokenErrorOnPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileRemoveFromCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileAddToCookieTwice);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           StartReconcileBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, StartReconcileOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, Lock);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMethodParamTest,
                           StartReconcileWithSessionInfoExpiredDefault);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMethodParamTest,
                           AccountReconcilorStateScheduled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           AddAccountToCookieCompletedWithBogusAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest, NoLoopWithBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           WontMergeAccountsWithError);
  FRIEND_TEST_ALL_PREFIXES(
      AccountReconcilorMirrorTest,
      WontMergeAccountsWithErrorDiscoveredByAccountReconcilorItself);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, DelegateTimeoutIsCalled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorMirrorTest,
                           DelegateTimeoutIsNotCalled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ForcedReconcileTriggerShouldNotCallListAccounts);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           ForcedReconcileTriggerShouldNotResultInNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           DelegateTimeoutIsNotCalledIfTimeoutIsNotReached);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, MultiloginLogout);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestForceDiceMigration,
                           TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestActiveDirectory,
                           TableRowTestMultilogin);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ReconcileAfterShutdown);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, UnlockAfterShutdown);
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           OnAccountsInCookieUpdatedLogoutInProgress);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorThrottlerTest, RefillOneRequest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorThrottlerTest, RefillFiveRequests);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorThrottlerTest,
                           NewRequestParamsPasses);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorThrottlerTest, BlockFiveRequests);

  // Operation executed by the reconcilor.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Operation {
    kNoop = 0,
    kLogout = 1,
    kMultilogin = 2,
    kThrottled = 3,

    kMaxValue = kThrottled
  };

  // Event triggering a call to StartReconcile().
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(Trigger)
  enum class Trigger {
    kInitialized = 0,
    kTokensLoaded = 1,
    kEnableReconcile = 2,
    kUnblockReconcile = 3,
    kTokenChange = 4,
    kTokenChangeDuringReconcile = 5,
    kCookieChange = 6,
    kCookieSettingChange = 7,
    kForcedReconcile = 8,
    kPrimaryAccountChanged = 9,

    kMaxValue = kPrimaryAccountChanged
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SigninReconcilerTrigger)

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> timer);

  bool IsRegisteredWithIdentityManager() const {
    return registered_with_identity_manager_;
  }

  // Register and unregister with dependent services.
  void RegisterWithAllDependencies();
  void UnregisterWithAllDependencies();
  void RegisterWithIdentityManager();
  void UnregisterWithIdentityManager();
  void RegisterWithContentSettings();
  void UnregisterWithContentSettings();
#if BUILDFLAG(IS_CHROMEOS)
  // This registration with `AccountManagerFacade` is required to force an
  // account reconciliation when `OnSigninDialogClosed()` is received.
  // Currently, only ChromeOS provides this notification. Extend this to other
  // Mirror platforms after adding the relevant implementation of
  // `AccountManagerFacade` interface for that platform.
  void RegisterWithAccountManagerFacade();
  void UnregisterWithAccountManagerFacade();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // All actions with side effects, only doing meaningful work if account
  // consistency is enabled. Virtual so that they can be overridden in tests.
  virtual void PerformLogoutAllAccountsAction();
  virtual void PerformSetCookiesAction(
      const signin::MultiloginParameters& parameters);

  // Used during periodic reconciliation.
  void StartReconcile(Trigger trigger);
  // |gaia_accounts| are the accounts in the Gaia cookie.
  void FinishReconcile(const CoreAccountId& primary_account,
                       const std::vector<CoreAccountId>& chrome_accounts,
                       std::vector<gaia::ListedAccount>&& gaia_accounts);
  void AbortReconcile();
  void ScheduleStartReconcileIfChromeAccountsChanged();

#if BUILDFLAG(ENABLE_MIRROR)
  // Forces reconciliation. A reconciliation cycle is started immediately if it
  // is not already running, otherwise another forced reconciliation is
  // attempted after some time.
  void ForceReconcile();
#endif  // BUILDFLAG(ENABLE_MIRROR)

  // Returns the list of valid accounts from the TokenService.
  std::vector<CoreAccountId> LoadValidAccountsFromTokenService() const;

  // The reconcilor only starts when the token service is ready.
  bool IsIdentityManagerReady();

  // Overridden from content_settings::Observer.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Overridden from signin::IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnRefreshTokensLoaded() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;

  void FinishReconcileWithMultiloginEndpoint(
      const CoreAccountId& primary_account,
      const std::vector<CoreAccountId>& chrome_accounts,
      std::vector<gaia::ListedAccount>&& gaia_accounts);
  void CalculateIfMultiloginReconcileIsDone();

#if BUILDFLAG(IS_CHROMEOS)
  // Overridden from account_manager::AccountManagerFacade::Observer.
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;
  void OnAuthErrorChanged(const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;
  void OnSigninDialogClosed() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Lock related methods.
  void IncrementLockCount();
  void DecrementLockCount();
  void BlockReconcile();
  void UnblockReconcile();

  void HandleReconcileTimeout();

  // Returns true if current array of existing accounts in cookie is different
  // from the desired one. If this returns false, the multilogin call would be a
  // no-op.
  bool CookieNeedsUpdate(
      const signin::MultiloginParameters& parameters,
      const std::vector<gaia::ListedAccount>& existing_accounts);

  // Sets the reconcilor state and calls Observer::OnStateChanged() if needed.
  void SetState(signin_metrics::AccountReconcilorState state);

  // Returns whether Shutdown() was called.
  bool WasShutDown() const;

  static void RecordReconcileOperation(Trigger trigger, Operation operation);

  // Histogram names.
  static const char kOperationHistogramName[];
  static const char kTriggerLogoutHistogramName[];
  static const char kTriggerMultiloginHistogramName[];
  static const char kTriggerNoopHistogramName[];
  static const char kTriggerThrottledHistogramName[];

  std::unique_ptr<signin::AccountReconcilorDelegate> delegate_;
  AccountReconcilorThrottler throttler_;

  // The IdentityManager associated with this reconcilor.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The SigninClient associated with this reconcilor.
  raw_ptr<SigninClient> client_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefChangeRegistrar pref_observer_;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS)
  // On Ash, this is a pointer to `AccountManagerFacadeImpl`.
  // Note: On Lacros too, this is a pointer to `AccountManagerFacadeImpl`, and
  // not `ProfileAccountManager`. This was done to simplify the design since
  // this pointer is only used to observe the closure of the OS/Ash-level signin
  // dialog and nothing else. Reconsider this decision if this usage changes in
  // the future.
  raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool registered_with_identity_manager_ = false;
  bool registered_with_content_settings_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  bool registered_with_account_manager_facade_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // True while the reconcilor is busy checking or managing the accounts in
  // this profile.
  bool is_reconcile_started_ = false;
  base::Time reconcile_start_time_;
  Trigger trigger_ = Trigger::kInitialized;

  // True iff this is the first time the reconcilor is executing.
  bool first_execution_ = true;

  // 'Most severe' error encountered during the last attempt to reconcile. If
  // the last reconciliation attempt was successful, this will be
  // |GoogleServiceAuthError::State::NONE|.
  // Severity of an error is defined on the basis of
  // |GoogleServiceAuthError::IsPersistentError()| only, i.e. any persistent
  // error is considered more severe than all non-persistent errors, but
  // persistent (or non-persistent) errors do not have an internal severity
  // ordering among themselves.
  GoogleServiceAuthError error_during_last_reconcile_ =
      GoogleServiceAuthError::AuthErrorNone();

  // Used for Dice migration: migration can happen if the accounts are
  // consistent, which is indicated by reconcile being a no-op.
  bool reconcile_is_noop_ = true;

  // Used during reconcile action.
  bool set_accounts_in_progress_ = false;     // Progress of SetAccounts calls.
  bool log_out_in_progress_ = false;          // Progress of LogOut calls.
  bool chrome_accounts_changed_ = false;

  // Used for the Lock.
  // StartReconcile() is blocked while this is > 0.
  int account_reconcilor_lock_count_ = 0;
  // StartReconcile() should be started when the reconcilor is unblocked.
  bool reconcile_on_unblock_ = false;

  base::ObserverList<Observer, true>::Unchecked observer_list_;

  // A timer to set off reconciliation timeout handlers, if account
  // reconciliation does not happen in a given |timeout_| duration.
  // Any delegate that wants to use this feature must override
  // |AccountReconcilorDelegate::GetReconcileTimeout|.
  // Note: This is intended as a safeguard for delegates that want a 'guarantee'
  // of reconciliation completing within a finite time. It is technically
  // possible for account reconciliation to be running/waiting forever in cases
  // such as a network connection not being present.
  std::unique_ptr<base::OneShotTimer> timer_ =
      std::make_unique<base::OneShotTimer>();
  base::TimeDelta timeout_;

  // Greater than 0 when synced data is being deleted, and it is important to
  // not invalidate the primary token while this is happening.
  int synced_data_deletion_in_progress_count_ = 0;

  // Note: when the reconcilor is blocked with `BlockReconcile()` the state is
  // set to kScheduled rather than kInactive as this is only used to temporarily
  // suspend the reconcilor.
  signin_metrics::AccountReconcilorState state_ =
      signin_metrics::AccountReconcilorState::kInactive;

  // Set to true when Shutdown() is called.
  bool was_shut_down_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<signin::ConsistencyCookieManager> consistency_cookie_manager_;
#endif

  base::WeakPtrFactory<AccountReconcilor> weak_factory_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
