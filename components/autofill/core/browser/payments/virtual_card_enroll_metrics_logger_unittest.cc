// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

struct EnrollmentSourceVariation {
  VirtualCardEnrollmentSource source;
  std::string metric_suffix;
};

class VirtualCardEnrollMetricsLoggerSourceTest
    : public ::testing::TestWithParam<EnrollmentSourceVariation> {};

static auto kEnrollmentSourceVariations = ::testing::Values(
    EnrollmentSourceVariation{
        .source = VirtualCardEnrollmentSource::kUpstream,
        .metric_suffix = "Upstream",
    },
    EnrollmentSourceVariation{
        .source = VirtualCardEnrollmentSource::kDownstream,
        .metric_suffix = "Downstream",
    },
    EnrollmentSourceVariation{
        .source = VirtualCardEnrollmentSource::kSettingsPage,
        .metric_suffix = "SettingsPage",
    });

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardEnrollMetricsLoggerSourceTest,
    kEnrollmentSourceVariations,
    [](const ::testing::TestParamInfo<EnrollmentSourceVariation>& info) {
      return info.param.metric_suffix;
    });

// Expect CardArtImageAvailable to be recorded in OnCardArtAvailable().
TEST_P(VirtualCardEnrollMetricsLoggerSourceTest, LogsOnCardArtAvailable) {
  base::HistogramTester histogram_tester;

  VirtualCardEnrollMetricsLogger::OnCardArtAvailable(
      /*card_art_available=*/true, GetParam().source);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable." +
          GetParam().metric_suffix,
      true, 1);
}

// Expect Shown to be recorded in OnShown().
TEST_P(VirtualCardEnrollMetricsLoggerSourceTest, LogsOnShown) {
  base::HistogramTester histogram_tester;

  VirtualCardEnrollMetricsLogger::OnShown(GetParam().source,
                                          /*is_reshow=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Shown." + GetParam().metric_suffix,
      false, 1);
}

// Expect Shown to be recorded when `is_reshow` is set in OnShown().
TEST_P(VirtualCardEnrollMetricsLoggerSourceTest, LogsOnShownWhenIsReshow) {
  base::HistogramTester histogram_tester;

  VirtualCardEnrollMetricsLogger::OnShown(GetParam().source,
                                          /*is_reshow=*/true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Shown." + GetParam().metric_suffix,
      true, 1);
}

struct ReshowVariation {
  bool is_reshow;
  std::string metric_suffix;
};

struct StrikesVariation {
  bool previously_declined;
  std::string metric_suffix;
};

struct MetricVariation {
  using TupleT =
      std::tuple<EnrollmentSourceVariation, ReshowVariation, StrikesVariation>;
  EnrollmentSourceVariation enrollment;
  ReshowVariation reshow;
  StrikesVariation strikes;

  inline explicit MetricVariation(TupleT t)
      : enrollment(std::get<0>(t)),
        reshow(std::get<1>(t)),
        strikes(std::get<2>(t)) {}

  inline std::string TestName() const {
    return base::StrCat({enrollment.metric_suffix, "_", reshow.metric_suffix,
                         "_", strikes.metric_suffix});
  }
};

class VirtualCardEnrollMetricsLoggerMetricsVariationsTest
    : public testing::TestWithParam<MetricVariation::TupleT> {
 public:
  inline MetricVariation GetConfig() const {
    // Once gtest includes ::testing::ConvertGenerator() the ParamType can
    // become a MetricVariation directly instead of a tuple that we need to
    // convert.
    return MetricVariation(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardEnrollMetricsLoggerMetricsVariationsTest,
    ::testing::Combine(kEnrollmentSourceVariations,
                       ::testing::Values(
                           ReshowVariation{
                               .is_reshow = false,
                               .metric_suffix = "FirstShow",
                           },
                           ReshowVariation{
                               .is_reshow = true,
                               .metric_suffix = "Reshows",
                           }),
                       ::testing::Values(
                           StrikesVariation{
                               .previously_declined = false,
                               .metric_suffix = "WithNoPreviousStrike",
                           },
                           StrikesVariation{
                               .previously_declined = true,
                               .metric_suffix = "WithPreviousStrikes",
                           })),
    [](const testing::TestParamInfo<
        VirtualCardEnrollMetricsLoggerMetricsVariationsTest::ParamType>& info) {
      return MetricVariation(info.param).TestName();
    });

// Expect that Result is recorded in OnDismissed().
TEST_P(VirtualCardEnrollMetricsLoggerMetricsVariationsTest, LogsOnDismissed) {
  base::HistogramTester histogram_tester;

  VirtualCardEnrollMetricsLogger::OnDismissed(
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED,
      GetConfig().enrollment.source, GetConfig().reshow.is_reshow,
      GetConfig().strikes.previously_declined);

  std::string expected_metric_name =
      base::StrCat({"Autofill.VirtualCardEnrollBubble.Result.",
                    GetConfig().enrollment.metric_suffix, ".",
                    GetConfig().reshow.metric_suffix});
  histogram_tester.ExpectUniqueSample(
      expected_metric_name,
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {expected_metric_name, ".", GetConfig().strikes.metric_suffix}),
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED,
      1);
}

}  // namespace autofill
