// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/metrics_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_interstitials {

class MetricsHelperTest : public ::testing::Test {
 protected:
  void SetMetricsHelper(MetricsHelper::ReportDetails report_details) {
    metrics_helper_ =
        std::make_unique<MetricsHelper>(example_url_, report_details, nullptr);
  }

  void RecordUserDecision(MetricsHelper::Decision decision) {
    metrics_helper_->RecordUserDecision(decision);
  }

  void RecordUserInteraction(MetricsHelper::Interaction interaction) {
    metrics_helper_->RecordUserInteraction(interaction);
  }

  void RecordInterstitialShowDelay() {
    metrics_helper_->RecordInterstitialShowDelay();
  }

  void VerifyInterstitialHistograms(
      std::string expected_histogram,
      std::string expected_histogram_with_extra_suffix,
      std::optional<std::string> expected_histogram_with_extra_extra_suffix) {
    histogram_tester_.ExpectTotalCount(expected_histogram, 1);
    histogram_tester_.ExpectTotalCount(expected_histogram_with_extra_suffix, 1);
    if (expected_histogram_with_extra_extra_suffix) {
      histogram_tester_.ExpectTotalCount(
          *expected_histogram_with_extra_extra_suffix, 1);
    }
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<MetricsHelper> metrics_helper_;
  GURL example_url_{"https://warning.com"};
};

TEST_F(MetricsHelperTest, RecordUserDecisionTest) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "phishing";
  report_details.extra_suffix = "from_client_side_detection";
  report_details.extra_extra_suffix = "scam_experiment_verdict_1";
  SetMetricsHelper(/*report_details=*/report_details);

  RecordUserDecision(MetricsHelper::Decision::PROCEED);
  VerifyInterstitialHistograms(
      "interstitial.phishing.decision",
      "interstitial.phishing.decision.from_client_side_detection",
      "interstitial.phishing.decision.from_client_side_detection.scam_"
      "experiment_verdict_1");
}

TEST_F(MetricsHelperTest, RecordUserInteractionTest) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "phishing";
  report_details.extra_suffix = "from_client_side_detection";
  report_details.extra_extra_suffix = "scam_experiment_verdict_1";
  SetMetricsHelper(/*report_details=*/report_details);

  RecordUserInteraction(MetricsHelper::Interaction::SHOW_ADVANCED);
  VerifyInterstitialHistograms(
      "interstitial.phishing.interaction",
      "interstitial.phishing.interaction.from_client_side_detection",
      "interstitial.phishing.interaction.from_client_side_detection.scam_"
      "experiment_verdict_1");
}

TEST_F(MetricsHelperTest, RecordInterstitialShowDelay) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "phishing";
  report_details.extra_suffix = "from_client_side_detection";
  report_details.extra_extra_suffix = "scam_experiment_verdict_1";
  SetMetricsHelper(/*report_details=*/report_details);

  RecordInterstitialShowDelay();
  VerifyInterstitialHistograms(
      "interstitial.phishing.show_delay",
      "interstitial.phishing.show_delay.from_client_side_detection",
      "interstitial.phishing.show_delay.from_client_side_detection.scam_"
      "experiment_verdict_1");
  VerifyInterstitialHistograms(
      "interstitial.phishing.show_delay_long_range",
      "interstitial.phishing.show_delay_long_range.from_client_side_detection",
      "interstitial.phishing.show_delay_long_range.from_client_side_detection."
      "scam_experiment_verdict_1");
}

}  // namespace security_interstitials
