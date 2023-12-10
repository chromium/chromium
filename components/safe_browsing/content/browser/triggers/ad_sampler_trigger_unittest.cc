// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/triggers/ad_sampler_trigger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/browser/triggers/mock_trigger_manager.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

using testing::_;
using testing::Return;

namespace safe_browsing {

namespace {
const char kAdUrl[] = "https://tpc.safeframe.googlesyndication.com/safeframe/1";
const char kNonAdUrl[] = "https://foo.com/";
const char kAdName[] = "google_ads_iframe_1";
const char kNonAdName[] = "foo";
}  // namespace

class AdSamplerTriggerTest : public content::RenderViewHostTestHarness {
 public:
  AdSamplerTriggerTest() : task_runner_(new base::TestSimpleTaskRunner) {}
  ~AdSamplerTriggerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Enable any prefs required for the trigger to run.
    safe_browsing::RegisterProfilePrefs(prefs_.registry());
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);
  }

  void CreateTriggerWithFrequency(const size_t denominator) {
    safe_browsing::AdSamplerTrigger::CreateForWebContents(
        web_contents(), &trigger_manager_, &prefs_, nullptr, nullptr, nullptr);

    safe_browsing::AdSamplerTrigger* ad_sampler =
        safe_browsing::AdSamplerTrigger::FromWebContents(web_contents());
    ad_sampler->SetSamplerFrequencyForTest(denominator);

    // Give the trigger a test task runner that we can synchronize on.
    ad_sampler->SetTaskRunnerForTest(task_runner_);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 RenderFrameHost* frame) {
    return NavigationSimulator::NavigateAndCommitFromDocument(GURL(url), frame);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetPrimaryMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             const std::string& frame_name,
                                             RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    return NavigateFrame(url, subframe);
  }

  void WaitForTaskRunnerIdle() {
    task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  WebContentsKey web_contents_key() {
    return GetWebContentsKey(web_contents());
  }

  MockTriggerManager* get_trigger_manager() { return &trigger_manager_; }
  base::HistogramTester* get_histograms() { return &histograms_; }

 private:
  TestingPrefServiceSimple prefs_;
  MockTriggerManager trigger_manager_;
  base::HistogramTester histograms_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(AdSamplerTriggerTest, TriggerDisabledBySamplingFrequency) {
  // Make sure the trigger doesn't fire when the sampling frequency is set to
  // zero, which disables the trigger.
  CreateTriggerWithFrequency(kAdSamplerFrequencyDisabled);
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _, _, _))
      .Times(0);

  // This page contains two ads - one identifiable by its URL, the other by the
  // name of the frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  // Three navigations (main frame, two subframes). One frame with no ads, and
  // two skipped ad samples.
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      TRIGGER_CHECK, 3);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_NO_AD, 1);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_AD_SKIPPED_FOR_FREQUENCY, 2);
}

TEST_F(AdSamplerTriggerTest, PageWithNoAds) {
  // Make sure the trigger doesn't fire when there are no ads on the page.
  CreateTriggerWithFrequency(/*denominator=*/1);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Three navigations (main frame, two subframes), each with no ad.
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      TRIGGER_CHECK, 3);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_NO_AD, 3);
}

TEST_F(AdSamplerTriggerTest, PageWithMultipleAds) {
  // Make sure the trigger fires when there are ads on the page. We expect
  // one call for each ad detected.
  CreateTriggerWithFrequency(/*denominator=*/1);
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(TriggerType::AD_SAMPLE,
                                           web_contents(), _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(
                  TriggerType::AD_SAMPLE, web_contents_key(), _, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(
          MockTriggerManager::FinishCollectingThreatDetailsResult(true, true)));

  // This page contains two ads - one identifiable by its URL, the other by the
  // name of the frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  // Wait for any posted tasks to finish.
  WaitForTaskRunnerIdle();

  // Three navigations (main frame, two subframes). Main frame with no ads, and
  // two sampled ads
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      TRIGGER_CHECK, 3);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_NO_AD, 1);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      AD_SAMPLED, 2);
}

TEST_F(AdSamplerTriggerTest, ReportRejectedByTriggerManager) {
  // If the trigger manager rejects the report, we don't try to finish/send the
  // report.
  CreateTriggerWithFrequency(/*denominator=*/1);
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(TriggerType::AD_SAMPLE,
                                           web_contents(), _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(
                  TriggerType::AD_SAMPLE, web_contents_key(), _, _, _, _, _, _))
      .Times(0);

  // One ad on the page, identified by its URL.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Wait for any posted tasks to finish.
  WaitForTaskRunnerIdle();

  // Three navigations (main frame, two subframes). Two frames with no ads, and
  // one ad rejected by trigger manager.
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      TRIGGER_CHECK, 3);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_NO_AD, 2);
  get_histograms()->ExpectBucketCount(kAdSamplerTriggerActionMetricName,
                                      NO_SAMPLE_COULD_NOT_START_REPORT, 1);
}

TEST(AdSamplerTriggerTestFinch, FrequencyDenominatorFeature) {
  // Make sure that setting the frequency denominator via Finch params works as
  // expected, and that the default frequency is used when no Finch config is
  // given.
  EXPECT_EQ(kAdSamplerDefaultFrequency,
            AdSamplerTrigger::GetSamplerFrequencyDenominatorForTest());

  const size_t kDenominatorInt = 12345;

  base::FieldTrialParams feature_params;
  feature_params[std::string(
      safe_browsing::kAdSamplerFrequencyDenominatorParam)] =
      base::NumberToString(kDenominatorInt);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kAdSamplerTriggerFeature, feature_params);

  EXPECT_EQ(kDenominatorInt,
            AdSamplerTrigger::GetSamplerFrequencyDenominatorForTest());
}
}  // namespace safe_browsing
