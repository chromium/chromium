// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/suspicious_site_trigger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/triggers/mock_trigger_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace safe_browsing {

namespace {
const char kSuspiciousUrl[] = "https://suspicious.com/";
const char kCleanUrl[] = "https://foo.com/";
}  // namespace

class SuspiciousSiteTriggerTest : public content::RenderViewHostTestHarness {
 public:
  SuspiciousSiteTriggerTest() : task_runner_(new base::TestSimpleTaskRunner) {}
  ~SuspiciousSiteTriggerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Enable any prefs required for the trigger to run.
    safe_browsing::RegisterProfilePrefs(prefs_.registry());
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);
  }

  void CreateTrigger(bool monitor_mode) {
    safe_browsing::SuspiciousSiteTrigger::CreateForWebContents(
        web_contents(), &trigger_manager_, &prefs_, nullptr, nullptr,
        monitor_mode);
    safe_browsing::SuspiciousSiteTrigger* trigger =
        safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents());
    // Give the trigger a test task runner that we can synchronize on.
    trigger->SetTaskRunnerForTest(task_runner_);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 RenderFrameHost* frame) {
    GURL gurl(url);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(gurl, frame);
    navigation_simulator->Commit();
    RenderFrameHost* final_frame_host =
        navigation_simulator->GetFinalRenderFrameHost();
    return final_frame_host;
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild("subframe");
    return NavigateFrame(url, subframe);
  }

  void StartNewFakeLoad() {
    // This fakes a new LoadStart event in the trigger, since the navigation
    // simulator doesn't restart the load when we start a new navigation.
    safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents())
        ->DidStartLoading();
  }

  void FinishAllNavigations() {
    // Call the trigger's DidStopLoading event handler directly since it is not
    // called as part of the navigating individual frames.
    safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents())
        ->DidStopLoading();
  }

  void TriggerSuspiciousSite() {
    // Notify the trigger that a suspicious site was detected.
    safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents())
        ->SuspiciousSiteDetected();
  }

  void WaitForTaskRunnerIdle() {
    task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  // Checks the trigger event histogram and ensures that |event| happened
  // |count| times.
  void ExpectEventHistogramCount(const SuspiciousSiteTriggerEvent event,
                                 int count) {
    histograms_.ExpectBucketCount(kSuspiciousSiteTriggerEventMetricName,
                                  static_cast<int>(event), count);
  }

  // Checks the histogram that tracks what state the trigger was in when the
  // delay timer fired. Ensures that the trigger was in |state| and occured
  // |count| times.
  void ExpectDelayStateHistogramCount(
      const SuspiciousSiteTrigger::TriggerState state,
      int count) {
    histograms_.ExpectBucketCount(
        kSuspiciousSiteTriggerReportDelayStateMetricName,
        static_cast<int>(state), count);
  }

  // Checks the report rejection histogram and makes sure that |count| reports
  // were rejected for |reason|.
  void ExpectReportRejectionHistogramCount(const TriggerManagerReason reason,
                                           int count) {
    histograms_.ExpectBucketCount(
        kSuspiciousSiteTriggerReportRejectionMetricName,
        static_cast<int>(reason), count);
  }

  // Checks the report rejection histogram and makes sure it was empty,
  // indicating no errors occurred.
  void ExpectNoReportRejection() {
    histograms_.ExpectTotalCount(
        kSuspiciousSiteTriggerReportRejectionMetricName, 0);
  }

  MockTriggerManager* get_trigger_manager() { return &trigger_manager_; }

 private:
  TestingPrefServiceSimple prefs_;
  MockTriggerManager trigger_manager_;
  base::HistogramTester histograms_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(SuspiciousSiteTriggerTest, RegularPageNonSuspicious) {
  // In a normal case where there are no suspicious URLs on the page, the
  // trigger should not fire.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  // One page load start and finish. No suspicious sites and no reports sent.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectNoReportRejection();
}

TEST_F(SuspiciousSiteTriggerTest, SuspiciousHitDuringLoad) {
  // When a suspicious site is detected in the middle of a page load, a report
  // is created after the page load has finished.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  TriggerSuspiciousSite();
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  WaitForTaskRunnerIdle();

  // One page load start and finish. One suspicious site detected and one
  // report started and sent after the page finished loading.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 1);

  // Ensure the delay timer fired and it happened in the REPORT_STARTED state
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_DELAY_TIMER, 1);
  ExpectDelayStateHistogramCount(
      SuspiciousSiteTrigger::TriggerState::REPORT_STARTED, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_FINISHED, 1);
  ExpectNoReportRejection();
}

TEST_F(SuspiciousSiteTriggerTest, SuspiciousHitAfterLoad) {
  // When a suspicious site is detected in after a page load, a report is
  // created immediately.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();
  TriggerSuspiciousSite();

  WaitForTaskRunnerIdle();

  // One page load start and finish. One suspicious site detected and one
  // report started and sent.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 1);

  // Ensure the delay timer fired and it happened in the REPORT_STARTED state
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_DELAY_TIMER, 1);
  ExpectDelayStateHistogramCount(
      SuspiciousSiteTrigger::TriggerState::REPORT_STARTED, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_FINISHED, 1);
  ExpectNoReportRejection();
}

TEST_F(SuspiciousSiteTriggerTest, ReportRejectedByTriggerManager) {
  // If the trigger manager rejects the report then no report is sent.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(
          DoAll(SetArgPointee<6>(TriggerManagerReason::DAILY_QUOTA_EXCEEDED),
                Return(false)));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  TriggerSuspiciousSite();
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  WaitForTaskRunnerIdle();

  // One page load start and finish. One suspicious site detected but no report
  // is sent because it's rejected. Error stats should reflect the rejection.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 1);

  // Ensure no report was started or finished, and no delay timer fired.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_DELAY_TIMER, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_FINISHED, 0);

  // Ensure that starting a report failed, and it was rejected for the
  // expected reason (quota).
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_START_FAILED, 1);
  ExpectReportRejectionHistogramCount(
      TriggerManagerReason::DAILY_QUOTA_EXCEEDED, 1);
}

TEST_F(SuspiciousSiteTriggerTest, NewNavigationMidLoad_NotSuspicious) {
  // Exercise what happens when a new navigation begins in the middle of a page
  // load when no suspicious site is detected.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  // Begin a brand new load before the first one is finished.
  StartNewFakeLoad();
  FinishAllNavigations();

  // Two page load start events, but only one finish. No suspicious sites
  // detected and no reports sent.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 2);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectNoReportRejection();
}

TEST_F(SuspiciousSiteTriggerTest, NewNavigationMidLoad_Suspicious) {
  // Exercise what happens when a new navigation begins in the middle of a page
  // load when a suspicious site was detected. The report of the first site
  // must be cancelled because we were waiting for the first load to finish
  // before beginning the report.
  CreateTrigger(/*monitor_mode=*/false);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  // Trigger a suspicious site. We wait for this page load to finish before
  // creating the report.
  TriggerSuspiciousSite();
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  // Begin a brand new load before the first one is finished. This will cancel
  // the report that is queued.
  StartNewFakeLoad();
  FinishAllNavigations();

  // Two page load start events, but only one finish. One suspicious site
  // detected but no reports created because the report gets cancelled.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 2);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectNoReportRejection();

  // Ensure that the repot got cancelled by the second load.
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::PENDING_REPORT_CANCELLED_BY_LOAD, 1);
}

TEST_F(SuspiciousSiteTriggerTest, MonitorMode_NotSuspicious) {
  // Testing the trigger in monitoring mode, it should never send reports.
  // In a normal case where there are no suspicious URLs on the page, the
  // trigger should not fire.
  CreateTrigger(/*monitor_mode=*/true);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  // One page load start and finish. No suspicious sites and no reports sent.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectNoReportRejection();
}

TEST_F(SuspiciousSiteTriggerTest, MonitorMode_SuspiciousHitDuringLoad) {
  // Testing the trigger in monitoring mode, it should never send reports.
  // When a suspicious site is detected in the middle of a page load, a report
  // is created after the page load has finished.
  CreateTrigger(/*monitor_mode=*/true);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  TriggerSuspiciousSite();
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  WaitForTaskRunnerIdle();

  // One page load start and finish. One suspicious site detected and one
  // possible report that gets skipped.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_START, 1);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED, 1);
  ExpectEventHistogramCount(
      SuspiciousSiteTriggerEvent::REPORT_POSSIBLE_BUT_SKIPPED, 1);

  // No reports are started or finished, no delay timer fired.
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_STARTED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_FINISHED, 0);
  ExpectEventHistogramCount(SuspiciousSiteTriggerEvent::REPORT_DELAY_TIMER, 0);
  ExpectNoReportRejection();
}
}  // namespace safe_browsing