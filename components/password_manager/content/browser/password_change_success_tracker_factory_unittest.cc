// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using UkmEntry = ukm::builders::PasswordManager_PasswordChangeFlowDuration;

namespace password_manager {

namespace {

constexpr char kUrl[] = "https://www.example.com";
constexpr char kUsername[] = "Paul";
constexpr bool kNotPhished = false;

}  // namespace

class PasswordChangeSuccessTrackerFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        browser_context());

    // Set up a fake preference service and register it to user prefs.
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordChangeSuccessTrackerVersion, 0);
    pref_service_.registry()->RegisterListPref(
        prefs::kPasswordChangeSuccessTrackerFlows);
    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(browser_context());
  }

 protected:
  content::TestBrowserContext* browser_context() { return &browser_context_; }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordChangeSuccessTrackerFactoryTest,
       CheckThatMetricsRecorderUmaIsSetUp) {
  base::HistogramTester histogram_tester;

  PasswordChangeSuccessTracker* tracker =
      PasswordChangeSuccessTrackerFactory::GetForBrowserContext(
          browser_context());

  tracker->OnManualChangePasswordFlowStarted(
      GURL(kUrl), kUsername,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  tracker->OnChangePasswordFlowModified(
      GURL(kUrl),
      PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow);

  tracker->OnChangePasswordFlowCompleted(
      GURL(kUrl), kUsername,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen,
      kNotPhished);

  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDurationV2.LeakCheckInSettings."
      "ManualHomepageFlow",
      1);

  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDurationV2.LeakCheckInSettings."
      "ManualHomepageFlow.ManualFlowPasswordChosen",
      1);
}

TEST_F(PasswordChangeSuccessTrackerFactoryTest,
       CheckThatMetricsRecorderUkmIsSetUp) {
  ukm::TestAutoSetUkmRecorder ukm_tester;

  PasswordChangeSuccessTracker* tracker =
      PasswordChangeSuccessTrackerFactory::GetForBrowserContext(
          browser_context());

  tracker->OnManualChangePasswordFlowStarted(
      GURL(kUrl), kUsername,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  tracker->OnChangePasswordFlowModified(
      GURL(kUrl),
      PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow);

  tracker->OnChangePasswordFlowCompleted(
      GURL(kUrl), kUsername,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen,
      kNotPhished);

  // Check that UKM logging is correct.
  const auto& entries = ukm_tester.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kStartEventName,
        static_cast<int64_t>(
            PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow));
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kEndEventName,
        static_cast<int64_t>(PasswordChangeSuccessTracker::EndEvent::
                                 kManualFlowGeneratedPasswordChosen));
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kEntryPointName,
        static_cast<int64_t>(
            PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings));
  }
}

}  // namespace password_manager
