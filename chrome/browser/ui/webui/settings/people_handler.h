// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "content/public/browser/web_contents_observer.h"

class LoginUIService;

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
                      public signin::IdentityManager::Observer,
                      public LoginUIService::LoginUI,
                      public syncer::SyncServiceObserver,
                      public content::WebContentsObserver {
 public:
  // TODO(tommycli): Remove these strings and instead use WebUIListener events.
  // These string constants are used from JavaScript (sync_browser_proxy.js).
  static const char kConfigurePageStatus[];
  static const char kDonePageStatus[];
  static const char kPassphraseFailedPageStatus[];

  explicit PeopleHandler(Profile* profile);
  ~PeopleHandler() override;

 protected:
  // Terminates the sync setup flow.
  void CloseSyncSetup();

  bool is_configuring_sync() const { return configuring_sync_; }

 private:
  friend class PeopleHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DisplayConfigureWithEngineDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      DisplayConfigureWithEngineDisabledAndCancelAfterSigninSuccess);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      DisplayConfigureWithEngineDisabledAndSyncStartupCompleted);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupCustomPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupTrustedVaultKeysRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupEncryptAll);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupManuallySyncAll);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupOldGaiaPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSetupSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           ShowSetupSyncForAllTypesIndividually);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSyncSetup);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, NonRegisteredType);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterCorrectExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           SuccessfullyCreateCustomPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterWrongExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, CannotCreateBlankPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           CannotCreatePassphraseIfEncryptEverythingDisallowed);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           CannotOverwritePassphraseWithNewOne);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, DisplayBasicLogin);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           AcquireSyncBlockerWhenLoadingSyncSettingsSubpage);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, RestartSyncAfterDashboardClear);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      RestartSyncAfterDashboardClearWithStandaloneTransport);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DashboardClearWhileSettingsOpen_ConfirmSoon);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DashboardClearWhileSettingsOpen_ConfirmLater);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerDiceUnifiedConsentTest,
                           StoredAccountsList);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerGuestModeTest, GetStoredAccountsList);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TurnOffSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, GetStoredAccountsList);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // LoginUIService::LoginUI implementation.
  void FocusUI() override;

  // IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // content::WebContentsObserver implementation
  void BeforeUnloadDialogCancelled() override;

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  std::unique_ptr<base::DictionaryValue> GetSyncStatusDictionary() const;

  // Helper routine that gets the SyncService associated with the parent
  // profile.
  syncer::SyncService* GetSyncService() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::ListValue* args);
  void OnDidClosePage(const base::ListValue* args);
  void HandleSetDatatypes(const base::ListValue* args);
  void HandleSetEncryptionPassphrase(const base::ListValue* args);
  void HandleSetDecryptionPassphrase(const base::ListValue* args);
  void HandleShowSyncSetupUI(const base::ListValue* args);
  void HandleSyncPrefsDispatch(const base::ListValue* args);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void HandleAttemptUserExit(const base::ListValue* args);
  void HandleTurnOnSync(const base::ListValue* args);
  void HandleTurnOffSync(const base::ListValue* args);
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void HandleStartSignin(const base::ListValue* args);
  void HandleSignout(const base::ListValue* args);
  void HandlePauseSync(const base::ListValue* args);
#endif
  void HandleStartKeyRetrieval(const base::ListValue* args);
  void HandleGetSyncStatus(const base::ListValue* args);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Displays the GAIA login form.
  void DisplayGaiaLogin(signin_metrics::AccessPoint access_point);

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point);
#endif

  void HandleGetStoredAccounts(const base::ListValue* args);
  void HandleStartSyncingWithEmail(const base::ListValue* args);
  base::Value GetStoredAccountsList();

  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Sends the current sync status to the JavaScript WebUI code.
  void UpdateSyncStatus();

  // Suppresses any further signin promos, since the user has signed in once.
  void MarkFirstSetupComplete();

  // If sync is indeed being configured, sets |configuring_sync_| to true.
  void MaybeMarkSyncConfiguring();

  // True if profile needs authentication before sync can run.
  bool IsProfileAuthNeededOrHasErrors();

  // If we're directly loading the sync setup page, we acquire a
  // SetupInProgressHandle early in order to prevent a lapse in SyncService's
  // "SetupInProgress" status. This lapse previously occurred between when the
  // sync confirmation dialog was closed and when the sync setup page hadn't yet
  // fired the SyncSetupShowSetupUI event. InitializeSyncBlocker is responsible
  // for checking if we're navigating to the setup page and acquiring the
  // |sync_blocker_|.
  void InitializeSyncBlocker();

  // Weak pointer.
  Profile* profile_;

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
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::WeakPtrFactory<PeopleHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PeopleHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_
