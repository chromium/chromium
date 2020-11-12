// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class TranslateMetricsLoggerImplTest : public testing::Test {
 public:
  void ResetTest() {
    translate_metrics_logger_ =
        std::make_unique<translate::TranslateMetricsLoggerImpl>(
            nullptr /*translate_manager*/);

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUp() override { ResetTest(); }

  translate::TranslateMetricsLoggerImpl* translate_metrics_logger() {
    return translate_metrics_logger_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void CheckTranslateStateHistograms(
      translate::TranslateState expected_initial_state,
      translate::TranslateState expected_final_state,
      int expected_num_translations,
      int expected_num_reversions) {
    histogram_tester()->ExpectUniqueSample(
        translate::kTranslatePageLoadInitialState, expected_initial_state, 1);
    histogram_tester()->ExpectUniqueSample(
        translate::kTranslatePageLoadFinalState, expected_final_state, 1);
    histogram_tester()->ExpectUniqueSample(
        translate::kTranslatePageLoadNumTranslations, expected_num_translations,
        1);
    histogram_tester()->ExpectUniqueSample(
        translate::kTranslatePageLoadNumReversions, expected_num_reversions, 1);
  }

 private:
  // Test target.
  std::unique_ptr<translate::TranslateMetricsLoggerImpl>
      translate_metrics_logger_;

  // Records the UMA histograms for each test.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(TranslateMetricsLoggerImplTest, MultipleRecordMetrics) {
  // Set test constants and log them with the test target.
  translate::RankerDecision ranker_decision =
      translate::RankerDecision::kShowUI;
  uint32_t ranker_model_version = 1234;

  translate::TriggerDecision trigger_decision =
      translate::TriggerDecision::kDisabledNeverTranslateLanguage;

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
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerDecision, ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerVersion, ranker_model_version, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadTriggerDecision, trigger_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadAutofillAssistantDeferredTriggerDecision,
      false, 1);
  CheckTranslateStateHistograms(
      translate::TranslateState::kNotTranslatedNoUI,
      translate::TranslateState::kNotTranslatedUIShown, 1, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogRankerMetrics) {
  translate::RankerDecision ranker_decision =
      translate::RankerDecision::kDontShowUI;
  uint32_t ranker_model_version = 4321;

  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerDecision, ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerVersion, ranker_model_version, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTriggerDecision) {
  // If we log multiple trigger decisions, we expect that only the first one is
  // recorded.
  std::vector<translate::TriggerDecision> trigger_decisions = {
      translate::TriggerDecision::kAutomaticTranslationByLink,
      translate::TriggerDecision::kDisabledByRanker,
      translate::TriggerDecision::kDisabledUnsupportedLanguage};

  for (auto trigger_decision : trigger_decisions)
    translate_metrics_logger()->LogTriggerDecision(trigger_decision);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadTriggerDecision, trigger_decisions[0], 1);
}

TEST_F(TranslateMetricsLoggerImplTest,
       LogAutofillAssistantDeferredTriggerDecision) {
  translate::TriggerDecision trigger_decision =
      translate::TriggerDecision::kShowUI;

  // Simulate the autofill assistant running the first time.
  translate_metrics_logger()->LogAutofillAssistantDeferredTriggerDecision();
  translate_metrics_logger()->LogTriggerDecision(trigger_decision);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadTriggerDecision, trigger_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadAutofillAssistantDeferredTriggerDecision,
      true, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTranslationAndReversion) {
  // Simulate a page load where the user translates a page and it is successful.
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kTranslatedNoUI, 1,
                                0);

  // Simulate a failed translation.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kNotTranslatedNoUI,
                                0, 0);

  // Simulate a translation that does not finish.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kNotTranslatedNoUI,
                                0, 0);

  // Simulate translating an already translated page, but the second translation
  // fails.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(true);
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kTranslatedNoUI, 1,
                                0);

  // Simulate the page being auto translated. Note that in this case the
  // translation will be queued before we mark the initial state, but in general
  // the translation will not finish until after. If the translation is
  // successful, then we still want to record the initial state as translated.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(true);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kTranslatedNoUI,
                                translate::TranslateState::kTranslatedNoUI, 1,
                                0);

  // Simulate an auto translation where the translation fails.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(false);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kNotTranslatedNoUI,
                                0, 0);

  // Simulate an auto translation where the translation does not finish.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kNotTranslatedNoUI,
                                0, 0);

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

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kNotTranslatedNoUI,
                                num_translations_and_reversions,
                                num_translations_and_reversions);
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

  CheckTranslateStateHistograms(translate::TranslateState::kNotTranslatedNoUI,
                                translate::TranslateState::kTranslatedUIShown,
                                1, 0);

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

  CheckTranslateStateHistograms(translate::TranslateState::kTranslatedUIShown,
                                translate::TranslateState::kNotTranslatedNoUI,
                                1, 1);
}
