// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace testing {

namespace {
const char* kAllUkmMetricNames[] = {
    ukm::builders::TranslatePageLoad::kSequenceNumberName,
    ukm::builders::TranslatePageLoad::kTriggerDecisionName,
    ukm::builders::TranslatePageLoad::kRankerDecisionName,
    ukm::builders::TranslatePageLoad::kRankerVersionName,
    ukm::builders::TranslatePageLoad::kInitialStateName,
    ukm::builders::TranslatePageLoad::kFinalStateName,
    ukm::builders::TranslatePageLoad::kNumTranslationsName,
    ukm::builders::TranslatePageLoad::kNumReversionsName,
    ukm::builders::TranslatePageLoad::kInitialSourceLanguageName,
    ukm::builders::TranslatePageLoad::kFinalSourceLanguageName,
    ukm::builders::TranslatePageLoad::
        kInitialSourceLanguageInContentLanguagesName,
    ukm::builders::TranslatePageLoad::kInitialTargetLanguageName,
    ukm::builders::TranslatePageLoad::kFinalTargetLanguageName,
    ukm::builders::TranslatePageLoad::kNumTargetLanguageChangesName,
    ukm::builders::TranslatePageLoad::kFirstUIInteractionName,
    ukm::builders::TranslatePageLoad::kNumUIInteractionsName,
    ukm::builders::TranslatePageLoad::kFirstTranslateErrorName,
    ukm::builders::TranslatePageLoad::kNumTranslateErrorsName,
    ukm::builders::TranslatePageLoad::kTotalTimeTranslatedName,
    ukm::builders::TranslatePageLoad::kTotalTimeNotTranslatedName,
    ukm::builders::TranslatePageLoad::kMaxTimeToTranslateName};
}  // namespace

class TranslateMetricsLoggerImplTest : public ::testing::Test {
 public:
  void ResetTest() {
    translate_metrics_logger_ = std::make_unique<TranslateMetricsLoggerImpl>(
        nullptr /*translate_manager*/);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUp() override { ResetTest(); }

  TranslateMetricsLoggerImpl* translate_metrics_logger() {
    return translate_metrics_logger_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

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

  // Helper functions to check that the metrics in the given UKM entry match
  // expectations.
  void CheckUkmEntrySequenceNumber(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_sequence_number) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kSequenceNumberName),
              expected_sequence_number);
  }

  void CheckUkmEntryTriggerDecision(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      TriggerDecision expected_trigger_decision) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kTriggerDecisionName),
              int(expected_trigger_decision));
  }

  void CheckUkmEntryRankerDecision(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      RankerDecision expected_ranker_decision) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kRankerDecisionName),
              int(expected_ranker_decision));
  }

  void CheckUkmEntryRankerVersion(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      uint32_t expected_ranker_version) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kRankerVersionName),
              int(expected_ranker_version));
  }

  void CheckUkmEntryInitialState(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      TranslateState expected_initial_state) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kInitialStateName),
              int(expected_initial_state));
  }

  void CheckUkmEntryFinalState(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      TranslateState expected_final_state) {
    EXPECT_EQ(
        ukm_entry.metrics.at(ukm::builders::TranslatePageLoad::kFinalStateName),
        int(expected_final_state));
  }

  void CheckUkmEntryNumTranslations(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_num_translations) {
    EXPECT_EQ(
        ukm_entry.metrics.at(
            ukm::builders::TranslatePageLoad::kNumTranslationsName),
        ukm::GetExponentialBucketMinForCounts1000(expected_num_translations));
  }

  void CheckUkmEntryNumReversions(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_num_reversions) {
    EXPECT_EQ(
        ukm_entry.metrics.at(
            ukm::builders::TranslatePageLoad::kNumReversionsName),
        ukm::GetExponentialBucketMinForCounts1000(expected_num_reversions));
  }

  void CheckUkmEntryInitialSourceLanguage(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const std::string& expected_initial_source_language) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kInitialSourceLanguageName),
              int(base::HashMetricName(expected_initial_source_language)));
  }

  void CheckUkmEntryFinalSourceLanguage(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const std::string& expected_final_source_language) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kFinalSourceLanguageName),
              int(base::HashMetricName(expected_final_source_language)));
  }

  void CheckUkmEntryInitialSourceLanguageInContentLanguages(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      bool expected_is_initial_source_language_in_content_languages) {
    EXPECT_EQ(
        ukm_entry.metrics.at(ukm::builders::TranslatePageLoad::
                                 kInitialSourceLanguageInContentLanguagesName),
        int(expected_is_initial_source_language_in_content_languages));
  }

  void CheckUkmEntryInitialTargetLanguage(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const std::string& expected_initial_target_language) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kInitialTargetLanguageName),
              int(base::HashMetricName(expected_initial_target_language)));
  }

  void CheckUkmEntryFinalTargetLanguage(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const std::string& expected_final_target_language) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kFinalTargetLanguageName),
              int(base::HashMetricName(expected_final_target_language)));
  }

  void CheckUkmEntryNumTargetLanguageChanges(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_num_target_language_changes) {
    EXPECT_EQ(
        ukm_entry.metrics.at(
            ukm::builders::TranslatePageLoad::kNumTargetLanguageChangesName),
        ukm::GetExponentialBucketMinForCounts1000(
            expected_num_target_language_changes));
  }

  void CheckUkmEntryFirstUIInteraction(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      UIInteraction expected_first_ui_interaction) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kFirstUIInteractionName),
              int(expected_first_ui_interaction));
  }

  void CheckUkmEntryNumUIInteractions(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_num_ui_interactions) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kNumUIInteractionsName),
              ukm::GetExponentialBucketMinForCounts1000(
                  expected_num_ui_interactions));
  }

  void CheckUkmEntryFirstTranslateError(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      TranslateErrors::Type expected_first_translate_error) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kFirstTranslateErrorName),
              int(expected_first_translate_error));
  }

  void CheckUkmEntryNumTranslateErrors(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      int expected_num_translate_errors) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kNumTranslateErrorsName),
              ukm::GetExponentialBucketMinForCounts1000(
                  expected_num_translate_errors));
  }

  void CheckUkmEntryTotalTimeTranslated(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const base::TimeDelta& expected_total_time_translated) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kTotalTimeTranslatedName),
              ukm::GetExponentialBucketMinForUserTiming(
                  expected_total_time_translated.InSeconds()));
  }

  void CheckUkmEntryTotalTimeNotTranslated(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const base::TimeDelta& expected_total_time_not_translated) {
    EXPECT_EQ(
        ukm_entry.metrics.at(
            ukm::builders::TranslatePageLoad::kTotalTimeNotTranslatedName),
        ukm::GetExponentialBucketMinForUserTiming(
            expected_total_time_not_translated.InSeconds()));
  }

  void CheckUkmEntryMaxTimeToTranslate(
      const ukm::TestUkmRecorder::HumanReadableUkmEntry& ukm_entry,
      const base::TimeDelta& expected_max_time_to_translate) {
    EXPECT_EQ(ukm_entry.metrics.at(
                  ukm::builders::TranslatePageLoad::kMaxTimeToTranslateName),
              ukm::GetExponentialBucketMinForUserTiming(
                  expected_max_time_to_translate.InMilliseconds()));
  }

 private:
  // Needed to set up the test UKM recorder.
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Test target.
  std::unique_ptr<TranslateMetricsLoggerImpl> translate_metrics_logger_;

  // Records the UMA histograms for each test.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Record the UKM protos for each test.
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

TEST_F(TranslateMetricsLoggerImplTest, RecordUkmMetrics) {
  // Establish constants for this test.
  base::SimpleTestTickClock test_clock;

  constexpr base::TimeDelta delay1 = base::TimeDelta::FromSeconds(100);
  constexpr base::TimeDelta delay2 = base::TimeDelta::FromSeconds(200);
  constexpr base::TimeDelta delay3 = base::TimeDelta::FromSeconds(300);
  constexpr base::TimeDelta delay4 = base::TimeDelta::FromSeconds(400);
  constexpr base::TimeDelta delay5 = base::TimeDelta::FromSeconds(500);

  constexpr base::TimeDelta translation_delay1 =
      base::TimeDelta::FromSeconds(10);
  constexpr base::TimeDelta translation_delay2 =
      base::TimeDelta::FromSeconds(30);
  constexpr base::TimeDelta translation_delay3 =
      base::TimeDelta::FromSeconds(20);

  const ukm::SourceId ukm_source_id = 4321;

  const RankerDecision ranker_decision = RankerDecision::kShowUI;
  const uint32_t ranker_model_version = 1234;

  const TriggerDecision trigger_decision =
      TriggerDecision::kDisabledNeverTranslateSite;

  const std::string initial_source_language = "es";
  const bool is_initial_source_language_in_users_content_languages = true;
  const std::string final_source_language = "it";

  const std::string initial_target_language = "de";
  const std::string final_target_language = "fr";

  // Simulate a page load where the following happens: the Ranker decides to
  // show the translate UI, the user initiates a manual translation which
  // finishes without an error, the user reverts the translations, the user
  // changes the source and target language, the user starts another
  // translation but this one fails due to a network error, the user tries to
  // translate again and this time the translation succeeds, and then finally
  // the user closes the translate UI.
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);
  translate_metrics_logger()->OnPageLoadStart(true);
  translate_metrics_logger()->SetUkmSourceId(ukm_source_id);

  translate_metrics_logger()->LogInitialSourceLanguage(
      initial_source_language,
      is_initial_source_language_in_users_content_languages);
  translate_metrics_logger()->LogTargetLanguage(initial_target_language);
  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);
  translate_metrics_logger()->LogTriggerDecision(trigger_decision);
  translate_metrics_logger()->LogUIChange(true);
  translate_metrics_logger()->LogInitialState();

  test_clock.Advance(delay1);

  translate_metrics_logger()->LogUIInteraction(UIInteraction::kTranslate);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(translation_delay1);
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

  test_clock.Advance(delay2);

  translate_metrics_logger()->LogUIInteraction(UIInteraction::kRevert);
  translate_metrics_logger()->LogReversion();

  test_clock.Advance(delay3);

  translate_metrics_logger()->LogUIInteraction(
      UIInteraction::kChangeSourceLanguage);
  translate_metrics_logger()->LogSourceLanguage(final_source_language);
  translate_metrics_logger()->LogUIInteraction(
      UIInteraction::kChangeTargetLanguage);
  translate_metrics_logger()->LogTargetLanguage(final_target_language);
  translate_metrics_logger()->LogUIInteraction(UIInteraction::kTranslate);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(translation_delay2);
  translate_metrics_logger()->LogTranslationFinished(false,
                                                     TranslateErrors::NETWORK);

  test_clock.Advance(delay4);

  translate_metrics_logger()->LogUIInteraction(UIInteraction::kTranslate);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(translation_delay3);
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

  test_clock.Advance(delay5);

  translate_metrics_logger()->LogUIInteraction(
      UIInteraction::kCloseUIExplicitly);
  translate_metrics_logger()->LogUIChange(false);

  // Record stored metrics.
  translate_metrics_logger()->RecordMetrics(true);

  // Check that the recorded UKM proto matches expectations.
  auto ukm_entries = test_ukm_recorder()->GetEntries(
      ukm::builders::TranslatePageLoad::kEntryName,
      std::vector<std::string>(std::begin(kAllUkmMetricNames),
                               std::end(kAllUkmMetricNames)));

  // Expect that ukm_entries has one element.
  EXPECT_EQ(ukm_entries.size(), 1u);

  // Only element has a source_id of ukm_source_id
  EXPECT_EQ(ukm_entries[0].source_id, ukm_source_id);

  // Check each metric in the UKM entry.
  CheckUkmEntrySequenceNumber(ukm_entries[0], 0);
  CheckUkmEntryTriggerDecision(ukm_entries[0], trigger_decision);
  CheckUkmEntryRankerDecision(ukm_entries[0], ranker_decision);
  CheckUkmEntryRankerVersion(ukm_entries[0], ranker_model_version);
  CheckUkmEntryInitialState(ukm_entries[0],
                            TranslateState::kNotTranslatedUIShown);
  CheckUkmEntryFinalState(ukm_entries[0], TranslateState::kTranslatedNoUI);
  CheckUkmEntryNumTranslations(ukm_entries[0], 2);
  CheckUkmEntryNumReversions(ukm_entries[0], 1);
  CheckUkmEntryInitialSourceLanguage(ukm_entries[0], initial_source_language);
  CheckUkmEntryFinalSourceLanguage(ukm_entries[0], final_source_language);
  CheckUkmEntryInitialSourceLanguageInContentLanguages(
      ukm_entries[0], is_initial_source_language_in_users_content_languages);
  CheckUkmEntryInitialTargetLanguage(ukm_entries[0], initial_target_language);
  CheckUkmEntryFinalTargetLanguage(ukm_entries[0], final_target_language);
  CheckUkmEntryNumTargetLanguageChanges(ukm_entries[0], 1);
  CheckUkmEntryFirstUIInteraction(ukm_entries[0], UIInteraction::kTranslate);
  CheckUkmEntryNumUIInteractions(ukm_entries[0], 7);
  CheckUkmEntryFirstTranslateError(ukm_entries[0], TranslateErrors::NETWORK);
  CheckUkmEntryNumTranslateErrors(ukm_entries[0], 1);
  CheckUkmEntryTotalTimeTranslated(ukm_entries[0], delay2 + delay5);
  CheckUkmEntryTotalTimeNotTranslated(
      ukm_entries[0], delay1 + delay3 + delay4 + translation_delay1 +
                          translation_delay2 + translation_delay3);
  CheckUkmEntryMaxTimeToTranslate(ukm_entries[0], translation_delay3);
}

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);
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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kNotTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simulate a failed translation.
  ResetTest();
  translate_metrics_logger()->LogInitialState();

  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false,
                                                     TranslateErrors::NETWORK);

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogTranslationFinished(false,
                                                     TranslateErrors::NETWORK);

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckTranslateStateHistograms(TranslateState::kTranslatedNoUI,
                                TranslateState::kTranslatedNoUI, 1, 0);
  CheckTranslateErrors(TranslateErrors::NONE, 0);

  // Simulate an auto translation where the translation fails.
  ResetTest();
  translate_metrics_logger()->LogTranslationStarted();
  translate_metrics_logger()->LogInitialState();
  translate_metrics_logger()->LogTranslationFinished(false,
                                                     TranslateErrors::NETWORK);

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
    translate_metrics_logger()->LogTranslationFinished(true,
                                                       TranslateErrors::NONE);
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
    translate_metrics_logger()->LogTranslationFinished(
        translate_error_type == TranslateErrors::NONE, translate_error_type);
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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);
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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

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
  translate_metrics_logger()->LogTranslationFinished(true,
                                                     TranslateErrors::NONE);

  translate_metrics_logger()->RecordMetrics(true);

  CheckMaxTimeToTranslate(default_delay);

  // Simulate an error with the translation. In this case the value of the
  // maximum time to translate should stay zero.
  ResetTest();
  translate_metrics_logger()->SetInternalClockForTesting(&test_clock);

  translate_metrics_logger()->LogTranslationStarted();
  test_clock.Advance(default_delay);
  translate_metrics_logger()->LogTranslationFinished(false,
                                                     TranslateErrors::NETWORK);

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
        test.translate_error_type == TranslateErrors::NONE,
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
