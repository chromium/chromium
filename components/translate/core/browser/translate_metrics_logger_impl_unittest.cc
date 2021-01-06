// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
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

  void CheckTranslateErrors(TranslateErrors::Type first_translate_error_type,
                            int num_translate_errors) {
    EXPECT_EQ(translate_metrics_logger_->first_translate_error_type_,
              first_translate_error_type);
    EXPECT_EQ(translate_metrics_logger_->num_translate_errors_,
              num_translate_errors);
  }

  void CheckTotalTimeTranslated(base::TimeDelta total_time_translated,
                                base::TimeDelta total_time_not_translated) {
    EXPECT_EQ(translate_metrics_logger_->total_time_translated_,
              total_time_translated);
    EXPECT_EQ(translate_metrics_logger_->total_time_not_translated_,
              total_time_not_translated);
  }

  void CheckMaxTimeToTranslate(base::TimeDelta expected_max_time_to_translate) {
    EXPECT_EQ(translate_metrics_logger_->max_time_to_translate_,
              expected_max_time_to_translate);
  }

  void CheckUIInteractions(UIInteraction expected_first_ui_interaction,
                           int expected_num_ui_interactions) {
    EXPECT_EQ(translate_metrics_logger_->first_ui_interaction_,
              expected_first_ui_interaction);
    EXPECT_EQ(translate_metrics_logger_->num_ui_interactions_,
              expected_num_ui_interactions);
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
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);
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
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simulate a failed translation.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NETWORK);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);
  CheckTranslateErrors(TranslateErrors::NETWORK, 1);

  // Simulate a translation that does not finish.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simulate translating an already translated page, but the second translation
  // fails.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NETWORK);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);
  CheckTranslateErrors(TranslateErrors::NETWORK, 1);

  // Simulate the page being auto translated. Note that in this case the
  // translation will be queued before we mark the initial state, but in general
  // the translation will not finish until after. If the translation is
  // successful, then we still want to record the initial state as translated.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simulate an auto translation where the translation fails.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NETWORK);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);
  CheckTranslateErrors(TranslateErrors::NETWORK, 1);

  // Simulate an auto translation where the translation does not finish.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kNotTranslatedNoUI, 0, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simualte a page that is repeatedly translated and then reverted.
  ResetTest();

  int num_translations_and_reversions = 100;

  translate_metrics_logger()->LogInitialState();

  for (int i = 0; i < num_translations_and_reversions; i++) {
    translate_metrics_logger()->LogTranslationStarted();
    translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);
    translate_metrics_logger()->LogReversion();
  }

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(
      TranslateState::kNotTranslatedNoUI, TranslateState::kNotTranslatedNoUI,
      num_translations_and_reversions, num_translations_and_reversions);
  CheckTranslateErrors(TranslateErrors::NONE, 0);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTranslateErrors) {
  // Sets the sequences of errors to supply.
  const TranslateErrors::Type kTranslateErrorTypes[] = {
      TranslateErrors::NONE,
      TranslateErrors::NETWORK,
      TranslateErrors::INITIALIZATION_ERROR,
      TranslateErrors::NONE,
      TranslateErrors::UNSUPPORTED_LANGUAGE,
      TranslateErrors::TRANSLATION_ERROR,
      TranslateErrors::NONE,
      TranslateErrors::TRANSLATION_TIMEOUT,
      TranslateErrors::SCRIPT_LOAD_ERROR,
      TranslateErrors::NONE};

  // Simulates the translations with the predefined errors.
  for (auto translate_error_type : kTranslateErrorTypes) {
    translate_metrics_logger()->LogTranslationStarted();
    translate_metrics_logger()->LogTranslationFinished(translate_error_type);
  }

  translate_metrics_logger()->RecordMetrics(true);

  // We expect to capture the first non-NONE value, and the total number of
  // non-NONE errors.
  CheckTranslateErrors(TranslateErrors::NETWORK, 6);

  // The number of successful translations is equal to the number of NONE
  // errors.
  histogram_tester()->ExpectUniqueSample(kTranslatePageLoadNumTranslations, 4,
                                         1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTranslateState) {
  // The translate state is defined along three dimensions: 1) whether the page
  // is translated, 2) whether the omnibox icon is shown, and 3) whether the
  // translate UI (either infobar or bubble) are shown. First test going from an
  // initial state with all three dimensions false to a final state where all
  // three are true.
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);
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
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

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
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

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
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

  test_clock.Advance(delay3);

  // Record the stored metrics.
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the member variables match expectations.
  CheckTotalTimeTranslated(delay3, delay1 + delay2);
}

TEST_F(TranslateMetricsLoggerImplTest, LogSourceLanguage) {
  // Set constants for the test.
  std::string initial_source_language = "ru";
  bool is_initial_source_language_in_users_content_languages = true;
  std::vector<std::string> source_languages = {"en", "es", "fr", "it", "de"};

  // Log the source languages.
  translate_metrics_logger()->LogInitialSourceLanguage(
      initial_source_language,
      is_initial_source_language_in_users_content_languages);
  for (auto source_language : source_languages)
    translate_metrics_logger()->LogSourceLanguage(source_language);

  // Record the stored metrics.
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the histograms match expectations.
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadInitialSourceLanguage,
      base::HashMetricName(initial_source_language), 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadIsInitialSourceLanguageInUsersContentLanguages,
      is_initial_source_language_in_users_content_languages, 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadFinalSourceLanguage,
      base::HashMetricName(source_languages[source_languages.size() - 1]), 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogTargetLanguage) {
  // Set constants for the test.
  std::vector<std::string> target_languages = {"de", "en", "en", "de", "fr",
                                               "fr", "es", "it", "it", "es"};

  // We only care about changes in the target language, so if the language stays
  // the same, we don't count it.
  int num_target_language_changes = 6;

  // Log the target languages.
  for (auto target_language : target_languages)
    translate_metrics_logger()->LogTargetLanguage(target_language);

  // Record the stored metrics
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the histograms match expectations.
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadInitialTargetLanguage,
      base::HashMetricName(target_languages[0]), 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadFinalTargetLanguage,
      base::HashMetricName(target_languages[target_languages.size() - 1]), 1);
  histogram_tester()->ExpectUniqueSample(
      kTranslatePageLoadNumTargetLanguageChanges, num_target_language_changes,
      1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogMaxTimeToTranslate) {
  // Set constants for this test.
  base::SimpleTestTickClock test_clock;

  constexpr base::TimeDelta default_delay = base::TimeDelta::FromSeconds(100);
  constexpr base::TimeDelta zero_delay = base::TimeDelta::FromSeconds(0);

  // Simulate sucessfully translating a page.
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(default_delay);
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckMaxTimeToTranslate(default_delay);

  // Simulate an error with the translation. In this case the value of the
  // maximum time to translate should stay zero.
  ResetTest();
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(default_delay);
  translate_metrics_logger()->LogTranslationFinished(TranslateErrors::NETWORK);

  translate_metrics_logger()->RecordMetrics(true);

  CheckMaxTimeToTranslate(zero_delay);

  // Simulate a translation that doesn't finish. The maximum time to translate
  // should also be zero here.
  ResetTest();
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(default_delay);

  translate_metrics_logger()->RecordMetrics(true);

  CheckMaxTimeToTranslate(zero_delay);

  // Simulate multiple translations (some with errors). The maximum time to
  // translate should only be from translations without an error.i
  const struct {
    base::TimeDelta time_to_translate;
    TranslateErrors::Type translate_error_type;
  } kTests[] = {
      {base::TimeDelta::FromSeconds(100), TranslateErrors::NONE},
      {base::TimeDelta::FromSeconds(200), TranslateErrors::NETWORK},
      {base::TimeDelta::FromSeconds(400), TranslateErrors::NONE},
      {base::TimeDelta::FromSeconds(500), TranslateErrors::NETWORK},
      {base::TimeDelta::FromSeconds(300), TranslateErrors::NONE},
  };

  for (const auto& test : kTests) {
    translate_metrics_logger()->LogTranslationStarted();
    test_clock.Advance(test.time_to_translate);
    translate_metrics_logger()->LogTranslationFinished(
        test.translate_error_type);
  }

  translate_metrics_logger()->RecordMetrics(true);

  CheckMaxTimeToTranslate(base::TimeDelta::FromSeconds(400));
}

TEST_F(TranslateMetricsLoggerImplTest, LogUIInteraction) {
  const UIInteraction kUIInteractions[] = {
      UIInteraction::kTranslate,
      UIInteraction::kRevert,
      UIInteraction::kAlwaysTranslateLanguage,
      UIInteraction::kChangeSourceLanguage,
      UIInteraction::kChangeTargetLanguage,
      UIInteraction::kNeverTranslateLanguage,
      UIInteraction::kNeverTranslateSite,
      UIInteraction::kCloseUIExplicitly,
      UIInteraction::kCloseUILostFocus};
  for (auto ui_interaction : kUIInteractions) {
    translate_metrics_logger()->LogUIInteraction(ui_interaction);
  }

  translate_metrics_logger()->RecordMetrics(true);

  CheckUIInteractions(kUIInteractions[0], 9);
}

}  // namespace testing

}  // namespace translate
