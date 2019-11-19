// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"

namespace unified_consent {
namespace {

class UnifiedConsentBrowserTest : public SyncTest {
 public:
  UnifiedConsentBrowserTest() : SyncTest(TWO_CLIENT) {}
  ~UnifiedConsentBrowserTest() override = default;

  void EnableSync(int client_id) {
    InitializeSyncClientsIfNeeded();

    ASSERT_TRUE(GetClient(client_id)->SetupSync());
  }

  void StartSyncSetup(int client_id) {
    InitializeSyncClientsIfNeeded();

    sync_blocker_ = GetSyncService(client_id)->GetSetupInProgressHandle();
    ASSERT_TRUE(GetClient(client_id)->SignInPrimaryAccount());
    GetSyncService(client_id)->GetUserSettings()->SetSyncRequested(true);
    ASSERT_TRUE(GetClient(client_id)->AwaitEngineInitialization());
  }

  void FinishSyncSetup(int client_id) {
    GetSyncService(client_id)->GetUserSettings()->SetFirstSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
    sync_blocker_.reset();
    ASSERT_TRUE(GetClient(client_id)->AwaitSyncSetupCompletion());
  }

  UnifiedConsentService* consent_service() {
    return UnifiedConsentServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  void InitializeSyncClientsIfNeeded() {
    if (GetSyncClients().empty())
      ASSERT_TRUE(SetupClients());
  }

  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentBrowserTest);
};

// Tests that the settings histogram is recorded if unified consent is enabled.
// The histogram is recorded during profile initialization.
IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest, SettingsHistogram_None) {
  histogram_tester_.ExpectUniqueSample(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kNone, 1);
}

// Tests that all service entries in the settings histogram are recorded after
// enabling them.
IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    PRE_SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  EnableSync(0);
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
}

IN_PROC_BROWSER_TEST_F(
    UnifiedConsentBrowserTest,
    SettingsHistogram_UrlKeyedAnonymizedDataCollectionEnabled) {
  histogram_tester_.ExpectBucketCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kNone, 0);
  histogram_tester_.ExpectBucketCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings",
      metrics::SettingsHistogramValue::kUrlKeyedAnonymizedDataCollection, 1);
  histogram_tester_.ExpectTotalCount(
      "UnifiedConsent.SyncAndGoogleServicesSettings", 1);
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest,
                       SettingsOptInTakeOverServicePrefChanges) {
  std::string pref_A = prefs::kSearchSuggestEnabled;
  std::string pref_B = prefs::kAlternateErrorPagesEnabled;

  // First client: Enable sync.
  EnableSync(0);
  // First client: Turn off both prefs while sync is on, so the synced state of
  // both prefs will be "off".
  GetProfile(0)->GetPrefs()->SetBoolean(pref_A, false);
  GetProfile(0)->GetPrefs()->SetBoolean(pref_B, false);
  // Make sure the updates are committed before proceeding with the test.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Second client: Turn off both prefs while sync is off.
  GetProfile(1)->GetPrefs()->SetBoolean(pref_A, false);
  GetProfile(1)->GetPrefs()->SetBoolean(pref_B, false);

  // Second client: Turn on pref A while sync is off.
  GetProfile(1)->GetPrefs()->SetBoolean(pref_A, true);

  // Second client: Start sync setup.
  StartSyncSetup(1);
  ASSERT_TRUE(GetSyncService(1)->IsSetupInProgress());
  ASSERT_FALSE(GetSyncService(1)->GetUserSettings()->IsFirstSetupComplete());

  // Second client: Turn on pref B while sync setup is in progress.
  GetProfile(1)->GetPrefs()->SetBoolean(pref_B, true);

  // Second client: Finish sync setup.
  FinishSyncSetup(1);

  // Sync both clients, so the synced state of both prefs (i.e. off) will arrive
  // at the second client.
  AwaitQuiescence();

  // Both clients: Expect that pref A is off and pref B is on.
  // Reason:
  // - Pref A was turned on before sync was enabled, hence it is overridden by
  // the value of the first client.
  // - Pref B was turned on while sync setup was in progress, hence it is taken
  // over.
  EXPECT_FALSE(GetProfile(0)->GetPrefs()->GetBoolean(pref_A));
  EXPECT_TRUE(GetProfile(0)->GetPrefs()->GetBoolean(pref_B));
  EXPECT_FALSE(GetProfile(1)->GetPrefs()->GetBoolean(pref_A));
  EXPECT_TRUE(GetProfile(1)->GetPrefs()->GetBoolean(pref_B));
}

}  // namespace
}  // namespace unified_consent
