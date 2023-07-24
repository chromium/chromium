// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/browser_test.h"

namespace unified_consent {
namespace {

class UnifiedConsentSyncToSigninBrowserTest : public SyncTest {
 public:
  UnifiedConsentSyncToSigninBrowserTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);
  }

  UnifiedConsentSyncToSigninBrowserTest(
      const UnifiedConsentSyncToSigninBrowserTest&) = delete;
  UnifiedConsentSyncToSigninBrowserTest& operator=(
      const UnifiedConsentSyncToSigninBrowserTest&) = delete;

  ~UnifiedConsentSyncToSigninBrowserTest() override = default;

  syncer::SyncServiceImpl* GetSyncService() {
    return SyncTest::GetSyncService(0);
  }

  UnifiedConsentService* GetUnifiedConsentService() {
    return UnifiedConsentServiceFactory::GetForProfile(GetProfile(0));
  }

  bool IsAnonymizedDataCollectionEnabled() const {
    auto helper = UrlKeyedDataCollectionConsentHelper::
        NewAnonymizedDataCollectionConsentHelper(GetProfile(0)->GetPrefs());
    return helper->IsEnabled();
  }

  void SignIn() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  void SignOut() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    signin::ClearPrimaryAccount(identity_manager);
  }

  bool WaitForPassphraseRequired() {
    return PassphraseRequiredChecker(GetSyncService()).Wait();
  }

  bool WaitForPassphraseAccepted() {
    return PassphraseAcceptedChecker(GetSyncService()).Wait();
  }

  bool WaitForNigoriOnServer(syncer::PassphraseType expected_passphrase_type) {
    return ServerPassphraseTypeChecker(expected_passphrase_type).Wait();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       EnablesAnonymizedDataCollectionOnSignInWithHistory) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // During a previous signin, the user had opted in to history.
  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  SignOut();

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // Now the user signs in again.
  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // AnonymizedDataCollection should have gotten enabled, because history is on
  // (and there's no custom passphrase).
  EXPECT_TRUE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       EnablesAnonymizedDataCollectionOnHistoryOptIn) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // AnonymizedDataCollection did NOT get enabled yet, because there is no
  // history opt-in.
  ASSERT_FALSE(GetSyncService()->GetUserSettings()->GetSelectedTypes().Has(
      syncer ::UserSelectableType::kHistory));
  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // Opt in to history.
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  // AnonymizedDataCollection should've gotten enabled with the history opt-in.
  EXPECT_TRUE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       DisablesAnonymizedDataCollectionOnSignOut) {
  ASSERT_TRUE(SetupClients());

  // The user signs in and opts in to history.
  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  // AnonymizedDataCollection got enabled with the history opt-in.
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // The user signs out again.
  SignOut();

  // AnonymizedDataCollection should've gotten disabled again.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DoesNotEnableAnonymizedDataCollectionOnSignInWithHistoryAndCustomPassphrase) {
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // During a previous signin, the user had opted in to history.
  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  SignOut();

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());

  ASSERT_TRUE(WaitForPassphraseRequired());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(WaitForPassphraseAccepted());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->IsUsingExplicitPassphrase());

  // AnonymizedDataCollection should NOT have gotten enabled, even though
  // history is on, because there's a custom passphrase.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DoesNotEnableAnonymizedDataCollectionOnHistoryOptInWithCustomPassphrase) {
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // AnonymizedDataCollection did NOT get enabled yet, because there is no
  // history opt-in.
  ASSERT_FALSE(GetSyncService()->GetUserSettings()->GetSelectedTypes().Has(
      syncer ::UserSelectableType::kHistory));
  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // Opt in to history.
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  // AnonymizedDataCollection should NOT have gotten enabled, because there's
  // a custom passphrase.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DisablesAnonymizedDataCollectionWhenCustomPassphraseSetOnThisDevice) {
  ASSERT_TRUE(SetupClients());

  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // The user sets up a custom passphrase.
  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForNigoriOnServer(syncer::PassphraseType::kCustomPassphrase));
  ASSERT_TRUE(WaitForPassphraseAccepted());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->IsUsingExplicitPassphrase());

  // AnonymizedDataCollection should have gotten disabled.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DisablesAnonymizedDataCollectionWhenCustomPassphraseSetOnOtherDevice) {
  ASSERT_TRUE(SetupClients());

  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // The user sets up a custom passphrase on a different device.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(WaitForPassphraseRequired());

  // AnonymizedDataCollection should have gotten disabled.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    PRE_DisablesAnonymizedDataCollectionWhenCustomPassphraseSetOnOtherDeviceWhileNotRunning) {
  ASSERT_TRUE(SetupClients());

  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DisablesAnonymizedDataCollectionWhenCustomPassphraseSetOnOtherDeviceWhileNotRunning) {
  // The user sets up a custom passphrase on a different device, while this
  // instance of the browser isn't running.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  // Now this browser starts up, and detects that there's a new passphrase.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(WaitForPassphraseRequired());

  // AnonymizedDataCollection should have gotten disabled.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       PRE_PreservesUsersOptOutAfterRestart) {
  ASSERT_TRUE(SetupClients());

  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // AnonymizedDataCollection got enabled automatically, because history is on
  // and there's no custom passphrase.
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // The user explicitly chooses to disable AnonymizedDataCollection.
  GetUnifiedConsentService()->SetUrlKeyedAnonymizedDataCollectionEnabled(false);
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       PreservesUsersOptOutAfterRestart) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // AnonymizedDataCollection should still be disabled (even though history is
  // on and there's no custom passphrase) since that was the user's choice.
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    PRE_PreservesUsersOptInAfterRestartWithCustomPassphrase) {
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  SignIn();
  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  ASSERT_TRUE(WaitForPassphraseRequired());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(WaitForPassphraseAccepted());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->IsUsingExplicitPassphrase());

  // AnonymizedDataCollection did NOT get enabled, even though history is on,
  // because there's a custom passphrase.
  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // The user explicitly chooses to enable AnonymizedDataCollection.
  GetUnifiedConsentService()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       PreservesUsersOptInAfterRestartWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->IsUsingExplicitPassphrase());

  // AnonymizedDataCollection should still be enabled (even though there's a
  // custom passphrase) since that was the user's choice.
  EXPECT_TRUE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentSyncToSigninBrowserTest,
                       EnablesAnonymizedDataCollectionOnSignInWhileOffline) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(IsAnonymizedDataCollectionEnabled());

  // Make the server return an error, so that the engine can't get initialized.
  // This is similar to being offline at the moment of sign-in.
  GetFakeServer()->SetHttpError(net::HTTP_INTERNAL_SERVER_ERROR);

  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            GetSyncService()->GetTransportState());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());
  ASSERT_FALSE(
      GetSyncService()->GetUserSettings()->GetPassphraseType().has_value());

  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  // AnonymizedDataCollection should have been (optimistically) enabled, even
  // though the passphrase type isn't known yet.
  EXPECT_TRUE(IsAnonymizedDataCollectionEnabled());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    PRE_DisablesAnonymizedDataCollectionAfterSignInWhileOfflineWithCustomPassphrase) {
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());

  // Make the server return an error, so that the engine can't get initialized.
  // This is similar to being offline at the moment of sign-in.
  GetFakeServer()->SetHttpError(net::HTTP_INTERNAL_SERVER_ERROR);

  SignIn();
  ASSERT_FALSE(GetSyncService()->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            GetSyncService()->GetTransportState());
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());
  ASSERT_FALSE(
      GetSyncService()->GetUserSettings()->GetPassphraseType().has_value());

  GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  // At this point, AnonymizedDataCollection is (optimistically) enabled, since
  // though the passphrase type isn't known yet.
  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // Before the engine finishes initialization, the user restarts the browser.
  ASSERT_FALSE(GetSyncService()->IsEngineInitialized());
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentSyncToSigninBrowserTest,
    DisablesAnonymizedDataCollectionAfterSignInWhileOfflineWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(IsAnonymizedDataCollectionEnabled());

  // The previous server/connection error has been resolved, so the engine can
  // initialize now.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->IsUsingExplicitPassphrase());

  // AnonymizedDataCollection should've been disabled, since there's a custom
  // passphrase. (Note that it's not necessary for the user ot actually enter
  // the passphrase.)
  EXPECT_FALSE(IsAnonymizedDataCollectionEnabled());
}

}  // namespace
}  // namespace unified_consent
