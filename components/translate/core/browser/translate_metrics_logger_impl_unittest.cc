// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace testing {

class TranslateMetricsLoggerImplTest : public ::testing::Test {
 public:
  void ResetTest() {
    translate_metrics_logger_ = std::make_unique<TranslateMetricsLoggerImpl>(
        nullptr /*translate_manager*/);

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUp() override { ResetTest(); }

  TranslateMetricsLoggerImpl* translate_metrics_logger() {
    return translate_metrics_logger_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void CheckTranslateStateHistograms(TranslateState expected_initial_state,
                                     TranslateState expected_final_state,
                                     int expected_num_translations,
                                     int expected_num_reversions) {
    histogram_tester()->ExpectUniqueSample(kTranslatePageLoadInitialState,
                                           expected_initial_state, 1);
    histogram_tester()->ExpectUniqueSample(kTranslatePageLoadFinalState,
                                           expected_final_state, 1);
    histogram_tester()->ExpectUniqueSample(kTranslatePageLoadNumTranslations,
                                           expected_num_translations, 1);
    histogram_tester()->ExpectUniqueSample(kTranslatePageLoadNumReversions,
                                           expected_num_reversions, 1);
  }

  void CheckTotalTimeTranslated(base::TimeDelta total_time_translated,
                                base::TimeDelta total_time_not_translated) {
    EXPECT_EQ(translate_metrics_logger_->total_time_translated_,
              total_time_translated);
    EXPECT_EQ(translate_metrics_logger_->total_time_not_translated_,
              total_time_not_translated);
  }

 private:
  // Test target.
  std::unique_ptr<TranslateMetricsLoggerImpl> translate_metrics_logger_;

  // Records the UMA histograms for each test.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(TranslateMetricsLoggerImplTest, MultipleRecordMetrics) {
  // Set test constants and log them with the test target.
  RankerDecision ranker_decision = RankerDecision::kShowUI;
  uint32_t ranker_model_version = 1234;

  TriggerDecision trigger_decision =
      TriggerDecision::kDisabledNeverTranslateLanguage;

  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);
  translate_metrics_logger()->LogTriggerDecision(trigger_decision);
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogUIChange(true);
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);
  translate_metrics_logger()->LogReversion();

  // Simulate |RecordMetrics| being called multiple times.
  translate_metrics_logger()->RecordMetrics(false);
  translate_metrics_logger()->RecordMetrics(false);
  translate_metrics_logger()->RecordMetrics(true);

  // The page-load UMA metrics should only be logged when the first
  // |RecordMetrics| is called. Subsequent calls shouldn't cause UMA metrics to
  // be logged.
  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadRankerDecision,
                                         ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadRankerVersion,
                                         ranker_model_version, 1);
  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadTriggerDecision,
                                         trigger_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadAutofillAssistantDeferredTriggerDecision, false, 1);
  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedUIShown, 1, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogRankerMetrics) {
  RankerDecision ranker_decision = RankerDecision::kDontShowUI;
  uint32_t ranker_model_version = 4321;

  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadRankerDecision,
                                         ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadRankerVersion,
                                         ranker_model_version, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTriggerDecision) {
  // If we log multiple trigger decisions, we expect that only the first one is
  // recorded.
  std::vector<TriggerDecision> trigger_decisions = {
      TriggerDecision::kAutomaticTranslationByLink,
      TriggerDecision::kDisabledByRanker,
      TriggerDecision::kDisabledUnsupportedLanguage};

  for (auto trigger_decision : trigger_decisions)
    translate_metrics_logger()->LogTriggerDecision(trigger_decision);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadTriggerDecision,
                                         trigger_decisions[0], 1);
}

TEST_F(TranslateMetricsLoggerImplTest,
       LogAutofillAssistantDeferredTriggerDecision) {
  TriggerDecision trigger_decision = TriggerDecision::kShowUI;

  // Simulate the autofill assistant running the first time.
  translate_metrics_logger()->LogAutofillAssistantDeferredTriggerDecision();
  translate_metrics_logger()->LogTriggerDecision(trigger_decision);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadTriggerDecision,
                                         trigger_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadAutofillAssistantDeferredTriggerDecision, true, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTranslationAndReversion) {
  // Simulate a page load where the user translates a page and it is successful.
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);

  // Simulate a failed translation.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);

  // Simulate a translation that does not finish.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);

  // Simulate translating an already translated page, but the second translation
  // fails.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);

  // Simulate the page being auto translated. Note that in this case the
  // translation will be queued before we mark the initial state, but in general
  // the translation will not finish until after. If the translation is
  // successful, then we still want to record the initial state as translated.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(true);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);

  // Simulate an auto translation where the translation fails.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);

  // Simulate an auto translation where the translation does not finish.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);

  // Simualte a page that is repeatedly translated and then reverted.
  ResetTest();

  int num_translations_and_reversions = 100;

  translate_metrics_logger()->LogInitialState();

  for (int i = 0; i < num_translations_and_reversions; i++) {
    translate_metrics_logger()->LogTranslationStarted();
    translate_metrics_logger()->LogTranslationFinished(true);
    translate_metrics_logger()->LogReversion();
  }

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(
      TranslateState::kNotTranslatedNoUI, TranslateState::kNotTranslatedNoUI,
      num_translations_and_reversions, num_translations_and_reversions);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTranslateState) {
  // The translate state is defined along three dimensions: 1) whether the page
  // is translated, 2) whether the omnibox icon is shown, and 3) whether the
  // translate UI (either infobar or bubble) are shown. First test going from an
  // initial state with all three dimensions false to a final state where all
  // three are true.
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);
  translate_metrics_logger()->LogUIChange(true);
  translate_metrics_logger()->LogOmniboxIconChange(true);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedUIShown, 1, 0);

  // Test going from an initial state with all three dimensions true to a final
  // state where all three are false.
  ResetTest();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogUIChange(true);
  translate_metrics_logger()->LogOmniboxIconChange(true);
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(true);

  translate_metrics_logger()->LogReversion();
  translate_metrics_logger()->LogUIChange(false);
  translate_metrics_logger()->LogOmniboxIconChange(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kTranslatedUIShown,
                                TranslateState::kNotTranslatedNoUI, 1, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, TrackTimeTranslatedAndNotTranslated) {
  // Set constants for this test.
  base::TimeDelta delay1 = base::TimeDelta::FromSeconds(100);
  base::TimeDelta delay2 = base::TimeDelta::FromSeconds(200);
  base::TimeDelta delay3 = base::TimeDelta::FromSeconds(300);
  base::TimeDelta delay4 = base::TimeDelta::FromSeconds(400);

  // Setup test clock, so it can be controlled by the test.
  base::SimpleTestTickClock test_clock;
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  // Page starts in the foreground and not translated.
  translate_metrics_logger()->OnPageLoadStart(true);

  test_clock.Advance(delay1);

  // Page switches to the background.
  translate_metrics_logger()->OnForegroundChange(false);

  test_clock.Advance(delay2);

  // Translate the page (while still in the background).
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);

  test_clock.Advance(delay3);

  // Page switches to the foreground.
  translate_metrics_logger()->OnForegroundChange(true);

  test_clock.Advance(delay4);

  // Record the stored metrics.
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the member variables match expectations.
  CheckTotalTimeTranslated(delay4, delay1);
}

TEST_F(TranslateMetricsLoggerImplTest,
       TrackTimeTranslatedAndNotTranslated_LongTranslation) {
  // Set constants for this test.
  base::TimeDelta delay1 = base::TimeDelta::FromSeconds(100);
  base::TimeDelta delay2 = base::TimeDelta::FromSeconds(200);
  base::TimeDelta delay3 = base::TimeDelta::FromSeconds(400);

  // Setup test clock, so it can be controlled by the test.
  base::SimpleTestTickClock test_clock;
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  // Page starts in the foreground and not translated.
  translate_metrics_logger()->OnPageLoadStart(true);

  test_clock.Advance(delay1);

  // Translation starts, but takes a while. We should count this time while the
  // translation is in progress as "not translated".
  translate_metrics_logger()->LogTranslationStarted();

  test_clock.Advance(delay2);

  // Translation finally finishes.
  translate_metrics_logger()->LogTranslationFinished(true);

  test_clock.Advance(delay3);

  // Record the stored metrics.
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the member variables match expectations.
  CheckTotalTimeTranslated(delay3, delay1 + delay2);
}

}  // namespace testing

}  // namespace translate
