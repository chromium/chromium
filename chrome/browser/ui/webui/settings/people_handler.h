// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/sync/driver/sync_service_observer.h"
#include "services/identity/public/cpp/identity_manager.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/account_tracker_service.h"
#endif

class LoginUIService;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace content {
class WebUI;
}  // namespace content

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace syncer {
class SyncSetupInProgressHandle;
}  // namespace syncer

namespace settings {

class PeopleHandler : public SettingsPageUIHandler,
                      public identity::IdentityManager::Observer,
                      public SyncStartupTracker::Observer,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                      public AccountTrackerService::Observer,
#endif
                      public LoginUIService::LoginUI,
                      public syncer::SyncServiceObserver {
 public:
  // TODO(tommycli): Remove these strings and instead use WebUIListener events.
  // These string constants are used from JavaScript (sync_browser_proxy.js).
  static const char kSpinnerPageStatus[];
  static const char kConfigurePageStatus[];
  static const char kTimeoutPageStatus[];
  static const char kDonePageStatus[];
  static const char kPassphraseFailedPageStatus[];

  explicit PeopleHandler(Profile* profile);
  ~PeopleHandler() override;

  // Terminates the sync setup flow.
  void CloseSyncSetup();

 protected:
  bool is_configuring_sync() const { return configuring_sync_; }

 private:
  friend class PeopleHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DisplayConfigureWithEngineDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      DisplayConfigureWithEngineDisabledAndCancelAfterSigninSuccess);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DisplayConfigureWithEngineDisabledAndSigninFailed);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      DisplayConfigureWithEngineDisabledAndSyncStartupCompleted);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, HandleSetupUIWhenSyncDisabled);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupCustomPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupEncryptAll);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupManuallySyncAll);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupOldGaiaPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupSyncForAllTypesIndividually);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSigninOnAuthError);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSyncSetup);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSyncSetupWhenNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestPassphraseStillRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           EnterExistingFrozenImplicitPassword);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, SetNewCustomPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterWrongExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterBlankExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TurnOnEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest, GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerFirstSigninTest, DisplayBasicLogin);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           AcquireSyncBlockerWhenLoadingSyncSettingsSubpage);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, RestartSyncAfterDashboardClear);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      RestartSyncAfterDashboardClearWithStandaloneTransport);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerDiceUnifiedConsentTest,
                           StoredAccountsList);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // SyncStartupTracker::Observer implementation.
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  // LoginUIService::LoginUI implementation.
  void FocusUI() override;

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // AccountTrackerService::Observer implementation.
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountImageUpdated(const std::string& account_id,
                             const gfx::Image& image) override;
  void OnAccountRemoved(const AccountInfo& info) override;
#endif

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  std::unique_ptr<base::DictionaryValue> GetSyncStatusDictionary();

  // Helper routine that gets the ProfileSyncService associated with the parent
  // profile.
  browser_sync::ProfileSyncService* GetSyncService() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::ListValue* args);
  void OnDidClosePage(const base::ListValue* args);
  void HandleSetDatatypes(const base::ListValue* args);
  void HandleSetEncryption(const base::ListValue* args);
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleAttemptUserExit(const base::ListValue* args);
#if defined(OS_CHROMEOS)
  void HandleRequestPinLoginState(const base::ListValue* args);
#endif
#if !defined(OS_CHROMEOS)
  void HandleStartSignin(const base::ListValue* args);
  void HandleSignout(const base::ListValue* args);
  void HandlePauseSync(const base::ListValue* args);
#endif
  void HandleGetSyncStatus(const base::ListValue* args);
  void HandleManageOtherPeople(const base::ListValue* args);
  void OnUnifiedConsentToggleChanged(const base::ListValue* args);

#if !defined(OS_CHROMEOS)
  // Displays the GAIA login form.
  void DisplayGaiaLogin(signin_metrics::AccessPoint access_point);

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point);
#endif

#if defined(OS_CHROMEOS)
  void OnPinLoginAvailable(bool is_available);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void HandleGetStoredAccounts(const base::ListValue* args);
  void HandleStartSyncingWithEmail(const base::ListValue* args);
  std::unique_ptr<base::ListValue> GetStoredAccountsList();
#endif

  // Displays spinner-only UI indicating that something is going on in the
  // background.
  // TODO(kochi): better to show some message that the user can understand what
  // is running in the background.
  void DisplaySpinner();

  // Displays an error dialog which shows timeout of starting the sync engine.
  void DisplayTimeout();

  // Closes the associated sync settings page.
  void CloseUI();

  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Sends the current sync status to the JavaScript WebUI code.
  void UpdateSyncStatus();

  // Suppresses any further signin promos, since the user has signed in once.
  void MarkFirstSetupComplete();

  // True if profile needs authentication before sync can run.
  bool IsProfileAuthNeededOrHasErrors();

  // If we're directly loading the sync setup page, we acquire a
  // SetupInProgressHandle early in order to prevent a lapse in
  // ProfileSyncService's "SetupInProgress" status. This lapse previously
  // occured between when the sync confirmation dialog was closed and when the
  // sync setup page hadn't yet fired the SyncSetupShowSetupUI event.
  // InitializeSyncBlocker is responsible for checking if we're navigating to
  // the setup page and acquiring the sync_blocker.
  void InitializeSyncBlocker();

  // Weak pointer.
  Profile* profile_;

  // Helper object used to wait for the sync engine to startup.
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // The OneShotTimer object used to timeout of starting the sync engine
  // service.
  std::unique_ptr<base::OneShotTimer> engine_start_timer_;

  // Used to listen for pref changes to allow or disallow signin.
  PrefChangeRegistrar profile_pref_registrar_;

  // Manages observer lifetimes.
  ScopedObserver<identity::IdentityManager, PeopleHandler>
      identity_manager_observer_;
  ScopedObserver<browser_sync::ProfileSyncService, PeopleHandler>
      sync_service_observer_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  ScopedObserver<AccountTrackerService, PeopleHandler>
      account_tracker_observer_;
#endif

#if defined(OS_CHROMEOS)
  base::WeakPtrFactory<PeopleHandler> weak_factory_{this};
#endif

  DISALLOW_COPY_AND_ASSIGN(PeopleHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_
