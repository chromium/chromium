// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"

namespace unified_consent {
namespace {

class UnifiedConsentBrowserTest : public SyncTest {
 public:
  UnifiedConsentBrowserTest() : SyncTest(TWO_CLIENT) {}

  UnifiedConsentBrowserTest(const UnifiedConsentBrowserTest&) = delete;
  UnifiedConsentBrowserTest& operator=(const UnifiedConsentBrowserTest&) =
      delete;

  ~UnifiedConsentBrowserTest() override = default;

  void EnableSync(int client_id) {
    InitializeSyncClientsIfNeeded();

    ASSERT_TRUE(GetClient(client_id)->SetupSync());
  }

  UnifiedConsentService* consent_service() {
    return UnifiedConsentServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::HistogramTester histogram_tester_;
  const std::string histogram_name_ =
      "UnifiedConsent.MakeSearchesAndBrowsingBetter.OnProfileLoad";

 private:
  void InitializeSyncClientsIfNeeded() {
    if (GetSyncClients().empty())
      ASSERT_TRUE(SetupClients());
  }
};

// Tests that the settings histogram is recorded if unified consent is enabled.
// The histogram is recorded during profile initialization.
IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest, SettingsHistogram_None) {
  histogram_tester_.ExpectUniqueSample(histogram_name_, false, 1);
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
  histogram_tester_.ExpectUniqueSample(histogram_name_, true, 1);
}

IN_PROC_BROWSER_TEST_F(UnifiedConsentBrowserTest,
                       SettingsOptInTakeOverServicePrefChanges) {
  std::string pref_A = prefs::kSearchSuggestEnabled;
  std::string pref_B = prefs::kSafeBrowsingEnabled;

  // First client: Enable sync.
  EnableSync(0);

  PrefService* pref_service0 = GetProfile(0)->GetPrefs();
  PrefService* pref_service1 = GetProfile(1)->GetPrefs();

  // First client: Turn off both prefs while sync is on, so the synced state of
  // both prefs will be "off".
  pref_service0->SetBoolean(pref_A, false);
  pref_service0->SetBoolean(pref_B, false);
  // Make sure the updates are committed before proceeding with the test.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Second client: Turn off both prefs while sync is off.
  pref_service1->SetBoolean(pref_A, false);
  pref_service1->SetBoolean(pref_B, false);

  // Second client: Turn on pref A while sync is off.
  pref_service1->SetBoolean(pref_A, true);

  // Second client: Turn on pref B while sync setup is in progress.
  ASSERT_TRUE(GetClient(1)->SetupSyncWithCustomSettings(
      base::BindLambdaForTesting([=](syncer::SyncUserSettings* settings) {
        ASSERT_FALSE(settings->IsInitialSyncFeatureSetupComplete());
        pref_service1->SetBoolean(pref_B, true);
        settings->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
      })));

  // Sync both clients, so the synced state of both prefs (i.e. off) will arrive
  // at the second client.
  ASSERT_TRUE(AwaitQuiescence());

  // Both clients: Expect that pref A is off and pref B is on.
  // Reason:
  // - Pref A was turned on before sync was enabled, hence it is overridden by
  // the value of the first client.
  // - Pref B was turned on while sync setup was in progress, hence it is taken
  // over.
  EXPECT_FALSE(pref_service0->GetBoolean(pref_A));
  EXPECT_TRUE(pref_service0->GetBoolean(pref_B));
  EXPECT_FALSE(pref_service1->GetBoolean(pref_A));
  EXPECT_TRUE(pref_service1->GetBoolean(pref_B));
}

}  // namespace
}  // namespace unified_consent
