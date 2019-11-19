// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_popup_trigger.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/mock_trigger_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock-generated-function-mockers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

using testing::_;
using testing::Return;

namespace safe_browsing {

namespace {
const char kAdUrl[] = "https://tpc.googlesyndication.com/safeframe/1";
const char kNonAdUrl[] = "https://foo.com/";
const char kAdName[] = "google_ads_iframe_1";
const char kNonAdName[] = "foo";
}  // namespace

class AdPopupTriggerTest : public content::RenderViewHostTestHarness {
 public:
  AdPopupTriggerTest() : task_runner_(new base::TestSimpleTaskRunner) {}
  ~AdPopupTriggerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Enable any prefs required for the trigger to run.
    safe_browsing::RegisterProfilePrefs(prefs_.registry());
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);
  }

  void CreateTrigger() {
    safe_browsing::AdPopupTrigger::CreateForWebContents(
        web_contents(), &trigger_manager_, &prefs_, nullptr, nullptr);

    safe_browsing::AdPopupTrigger* ad_popup_trigger =
        safe_browsing::AdPopupTrigger::FromWebContents(web_contents());

    // Give the trigger a test task runner that we can synchronize on.
    ad_popup_trigger->SetTaskRunnerForTest(task_runner_);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 RenderFrameHost* frame) {
    GURL gurl(url);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(gurl, frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigatePopup(const std::string& url,
                                          const std::string& frame_name,
                                          RenderFrameHost* parent) {
    RenderFrameHost* popup_opener_frame =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    RenderFrameHost* final_frame_host = NavigateFrame(url, popup_opener_frame);
    // Call the trigger's PopupWasBlocked event handler directly since it
    // doesn't happen as part of the navigation. This should check if the frame
    // opening the popup is an ad.
    safe_browsing::AdPopupTrigger::FromWebContents(web_contents())
        ->PopupWasBlocked(final_frame_host);
    return final_frame_host;
  }

  void WaitForTaskRunnerIdle() {
    task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  MockTriggerManager* get_trigger_manager() { return &trigger_manager_; }
  base::HistogramTester* get_histograms() { return &histograms_; }

 private:
  TestingPrefServiceSimple prefs_;
  MockTriggerManager trigger_manager_;
  base::HistogramTester histograms_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(AdPopupTriggerTest, PopupWithAds) {
  // Make sure the trigger fires when there are ads on the page.
  CreateTrigger();
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(
                  TriggerType::AD_POPUP, web_contents(), _, _, _, _, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(TriggerType::AD_POPUP,
                                            web_contents(), _, _, _, _))
      .Times(1);

  // This page contains two popups - one originating from an ad subframe and one
  // from a non ad subframe.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigatePopup(kNonAdUrl, kNonAdName, main_frame);
  CreateAndNavigatePopup(kNonAdUrl, kAdName, main_frame);

  // Wait for any posted tasks to finish.
  WaitForTaskRunnerIdle();

  // Two popup navigations, one will cause a report
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_CHECK, 2);
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_REPORTED, 1);
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_NO_GOOGLE_AD,
                                      1);
}

// TODO(https://crbug.com/1009917): Fix flakes on Windows bots.
#if defined(OS_WIN)
#define MAYBE_ReportRejectedByTriggerManager \
  DISABLED_ReportRejectedByTriggerManager
#else
#define MAYBE_ReportRejectedByTriggerManager ReportRejectedByTriggerManager
#endif
TEST_F(AdPopupTriggerTest, MAYBE_ReportRejectedByTriggerManager) {
  // If the trigger manager rejects the report, we don't try to finish/send the
  // report.
  CreateTrigger();
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(2);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kAdUrl);
  CreateAndNavigatePopup(kAdUrl, kAdName, main_frame);
  CreateAndNavigatePopup(kNonAdUrl, kAdName, main_frame);

  WaitForTaskRunnerIdle();

  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_CHECK, 2);
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_REPORTED, 0);
  get_histograms()->ExpectBucketCount(
      kAdPopupTriggerActionMetricName,
      AdPopupTriggerAction::POPUP_COULD_NOT_START_REPORT, 2);
}

TEST_F(AdPopupTriggerTest, DISABLED_PopupWithNoAds) {
  // Make sure the trigger doesn't fire when there are no ads on the page.
  CreateTrigger();
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigatePopup(kNonAdUrl, kNonAdName, main_frame);
  CreateAndNavigatePopup(kNonAdUrl, kNonAdName, main_frame);

  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_CHECK, 2);
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_NO_GOOGLE_AD,
                                      2);
}

TEST_F(AdPopupTriggerTest, PopupNotFromAdForPageWithAd) {
  // Make sure that no report is generated when there is an ad on the page but
  // the popup is caused from a different frame.
  CreateTrigger();
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetailsWithReason(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHostTester::For(main_frame)->AppendChild(kAdName);
  CreateAndNavigatePopup(kNonAdUrl, kNonAdName, main_frame);

  // Two navigations (main frame, one subframe), each with no ad.
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_CHECK, 1);
  get_histograms()->ExpectBucketCount(kAdPopupTriggerActionMetricName,
                                      AdPopupTriggerAction::POPUP_NO_GOOGLE_AD,
                                      1);
}

}  // namespace safe_browsing
