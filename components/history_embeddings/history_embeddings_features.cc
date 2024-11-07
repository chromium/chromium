// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace history_embeddings {

namespace {

FeatureParameters& GetFeatureParametersMutable() {
  static FeatureParameters parameters(true);
  return parameters;
}

}  // namespace

// Please use `IsFeatureManagementHistoryEmbeddingEnabled()` instead
// of using `kHistoryEmbeddings` directly.
BASE_FEATURE(kHistoryEmbeddings,
             "HistoryEmbeddings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Note: This feature has no parameters. Since it entirely depends on the
// above kHistoryEmbeddings feature, all parameters are owned by that
// feature to avoid confusion about which feature owns which parameters.
BASE_FEATURE(kHistoryEmbeddingsAnswers,
             "HistoryEmbeddingsAnswers",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kShowSourcePassages(&kHistoryEmbeddings,
                                                   "ShowSourcePassages",
                                                   false);

const base::FeatureParam<int> kPassageExtractionDelay(&kHistoryEmbeddings,
                                                      "PassageExtractionDelay",
                                                      5000);

const base::FeatureParam<int> kPassageExtractionMaxWordsPerAggregatePassage(
    &kHistoryEmbeddings,
    "PassageExtractionMaxWordsPerAggregatePassage",
    200);

const base::FeatureParam<int> kSearchQueryMinimumWordCount(
    &kHistoryEmbeddings,
    "SearchQueryMinimumWordCount",
    2);
const base::FeatureParam<int> kSearchPassageMinimumWordCount(
    &kHistoryEmbeddings,
    "SearchPassageMinimumWordCount",
    5);

// TODO(b/352384806): Take model metadata from Answerer when available,
//  and eliminate this parameter as it will then be unnecessary.
const base::FeatureParam<int> kContextPassagesMinimumWordCount(
    &kHistoryEmbeddings,
    "ContextPassagesMinimumWordCount",
    1000);

const base::FeatureParam<int> kSearchResultItemCount(&kHistoryEmbeddings,
                                                     "SearchResultItemCount",
                                                     3);

const base::FeatureParam<bool> kAtKeywordAcceleration(&kHistoryEmbeddings,
                                                      "AtKeywordAcceleration",
                                                      false);

const base::FeatureParam<double> kContentVisibilityThreshold(
    &kHistoryEmbeddings,
    "ContentVisibilityThreshold",
    0);

// See comment at `history_embeddings::GetScoreThreshold()`.
const base::FeatureParam<double> kSearchScoreThreshold(&kHistoryEmbeddings,
                                                       "SearchScoreThreshold",
                                                       -1);

// This one defaults true because we generally want it whenever v2 is enabled
// and it will only be used if applicable.
const base::FeatureParam<bool> kEnableIntentClassifier(&kHistoryEmbeddings,
                                                       "EnableIntentClassifier",
                                                       true);

const base::FeatureParam<bool> kUseMlIntentClassifier(&kHistoryEmbeddings,
                                                      "UseMlIntentClassifier",
                                                      false);

const base::FeatureParam<bool> kEnableMlIntentClassifierScore(
    &kHistoryEmbeddings,
    "EnableMlIntentClassifierScore",
    false);

const base::FeatureParam<int> kMockIntentClassifierDelayMS(
    &kHistoryEmbeddings,
    "MockIntentClassifierDelayMS",
    0);

const base::FeatureParam<bool> kUseMlAnswerer(&kHistoryEmbeddings,
                                              "UseMlAnswerer",
                                              false);

const base::FeatureParam<double> kMlAnswererMinScore(&kHistoryEmbeddings,
                                                     "MlAnswererMinScore",
                                                     0.5);

const base::FeatureParam<int> kMockAnswererDelayMS(&kHistoryEmbeddings,
                                                   "MockAnswererDelayMS",
                                                   0);

// Default to `ComputeAnswerStatus::SUCCESS`.
const base::FeatureParam<int> kMockAnswererStatus(&kHistoryEmbeddings,
                                                  "MockAnswererStatus",
                                                  2);

const base::FeatureParam<bool> kForceAnswererUseAllowed(
    &kHistoryEmbeddings,
    "ForceAnswererUseAllowed",
    false);

const base::FeatureParam<bool> kEnableImagesForResults(&kHistoryEmbeddings,
                                                       "EnableImagesForResults",
                                                       false);

const base::FeatureParam<bool> kOmniboxScoped(&kHistoryEmbeddings,
                                              "OmniboxScoped",
                                              true);

const base::FeatureParam<bool> kOmniboxUnscoped(&kHistoryEmbeddings,
                                                "OmniboxUnscoped",
                                                false);

const base::FeatureParam<bool> kAnswersInOmniboxScoped(&kHistoryEmbeddings,
                                                       "AnswersInOmniboxScoped",
                                                       false);

const base::FeatureParam<int> kScheduledEmbeddingsMax(&kHistoryEmbeddings,
                                                      "ScheduledEmbeddingsMax",
                                                      1);

const base::FeatureParam<bool> kSendQualityLog(&kHistoryEmbeddings,
                                               "SendQualityLog",
                                               false);
const base::FeatureParam<bool> kSendQualityLogV2(&kHistoryEmbeddings,
                                                 "SendQualityLogV2",
                                                 false);

const base::FeatureParam<int> kEmbedderNumThreads(&kHistoryEmbeddings,
                                                  "EmbeddingsNumThreads",
                                                  4);

const base::FeatureParam<int> kEmbedderCacheSize(&kHistoryEmbeddings,
                                                 "EmbedderCacheSize",
                                                 1000);

const base::FeatureParam<int> kMaxPassagesPerPage(&kHistoryEmbeddings,
                                                  "MaxPassagesPerPage",
                                                  30);

const base::FeatureParam<bool> kDeleteEmbeddings(&kHistoryEmbeddings,
                                                 "DeleteEmbeddings",
                                                 false);
const base::FeatureParam<bool> kRebuildEmbeddings(&kHistoryEmbeddings,
                                                  "RebuildEmbeddings",
                                                  true);

const base::FeatureParam<bool> kUseDatabaseBeforeEmbedder(
    &kHistoryEmbeddings,
    "UseDatabaseBeforeEmbedder",
    true);

const base::FeatureParam<bool> kUseUrlFilter(&kHistoryEmbeddings,
                                             "UseUrlFilter",
                                             false);

const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout(
    &kHistoryEmbeddings,
    "EmbeddingsServiceTimeout",
    base::Seconds(60));

const base::FeatureParam<bool> kEnableSidePanel(&kHistoryEmbeddings,
                                                "EnableSidePanel",
                                                false);

const base::FeatureParam<bool> kTrimAfterHostInResults(&kHistoryEmbeddings,
                                                       "TrimAfterHostInResults",
                                                       false);

const base::FeatureParam<int> kMaxAnswererContextUrlCount(
    &kHistoryEmbeddings,
    "MaxAnswererContextUrlCount",
    1);

const base::FeatureParam<double> kWordMatchMinEmbeddingScore(
    &kHistoryEmbeddings,
    "WordMatchMinEmbeddingScore",
    1.0);

const base::FeatureParam<int> kWordMatchMinTermLength(&kHistoryEmbeddings,
                                                      "WordMatchMinTermLength",
                                                      3);

const base::FeatureParam<double> kWordMatchScoreBoostFactor(
    &kHistoryEmbeddings,
    "WordMatchScoreBoostFactor",
    0.2);

const base::FeatureParam<int> kWordMatchLimit(&kHistoryEmbeddings,
                                              "WordMatchLimit",
                                              5);

const base::FeatureParam<int> kWordMatchSmoothingFactor(
    &kHistoryEmbeddings,
    "WordMatchSmoothingFactor",
    1);

const base::FeatureParam<int> kWordMatchMaxTermCount(&kHistoryEmbeddings,
                                                     "WordMatchMaxTermCount",
                                                     3);

const base::FeatureParam<double> kWordMatchRequiredTermRatio(
    &kHistoryEmbeddings,
    "WordMatchRequiredTermRatio",
    1.0);

const base::FeatureParam<bool> kScrollTagsEnabled(&kHistoryEmbeddings,
                                                  "ScrollTagsEnabled",
                                                  false);

const base::FeatureParam<bool> kEraseNonAsciiCharacters(
    &kHistoryEmbeddings,
    "EraseNonAsciiCharacters",
    false);

bool IsHistoryEmbeddingsEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return chromeos::features::IsFeatureManagementHistoryEmbeddingEnabled() &&
         base::FeatureList::IsEnabled(kHistoryEmbeddings);
#else
  return base::FeatureList::IsEnabled(kHistoryEmbeddings);
#endif
}

bool IsHistoryEmbeddingsAnswersEnabled() {
  return IsHistoryEmbeddingsEnabled() &&
         base::FeatureList::IsEnabled(kHistoryEmbeddingsAnswers);
}

FeatureParameters::FeatureParameters(bool load_finch) {
  if (!load_finch) {
    return;
  }
  show_source_passages = kShowSourcePassages.Get();
  passage_extraction_delay = kPassageExtractionDelay.Get();
  passage_extraction_max_words_per_aggregate_passage =
      kPassageExtractionMaxWordsPerAggregatePassage.Get();
  search_query_minimum_word_count = kSearchQueryMinimumWordCount.Get();
  search_passage_minimum_word_count = kSearchPassageMinimumWordCount.Get();
  context_passages_minimum_word_count = kContextPassagesMinimumWordCount.Get();
  search_result_item_count = kSearchResultItemCount.Get();
  at_keyword_acceleration = kAtKeywordAcceleration.Get();
  content_visibility_threshold = kContentVisibilityThreshold.Get();
  search_score_threshold = kSearchScoreThreshold.Get();
  enable_intent_classifier = kEnableIntentClassifier.Get();
  use_ml_intent_classifier = kUseMlIntentClassifier.Get();
  enable_ml_intent_classifier_score = kEnableMlIntentClassifierScore.Get();
  mock_intent_classifier_delay_ms = kMockIntentClassifierDelayMS.Get();
  use_ml_answerer = kUseMlAnswerer.Get();
  ml_answerer_min_score = kMlAnswererMinScore.Get();
  mock_answerer_delay_ms = kMockAnswererDelayMS.Get();
  mock_answerer_status = kMockAnswererStatus.Get();
  force_answerer_use_allowed = kForceAnswererUseAllowed.Get();
  enable_images_for_results = kEnableImagesForResults.Get();
  omnibox_scoped = kOmniboxScoped.Get();
  omnibox_unscoped = kOmniboxUnscoped.Get();
  answers_in_omnibox_scoped = kAnswersInOmniboxScoped.Get();
  scheduled_embeddings_max = kScheduledEmbeddingsMax.Get();
  send_quality_log = kSendQualityLog.Get();
  send_quality_log_v2 = kSendQualityLogV2.Get();
  embedder_num_threads = kEmbedderNumThreads.Get();
  embedder_cache_size = kEmbedderCacheSize.Get();
  max_passages_per_page = kMaxPassagesPerPage.Get();
  delete_embeddings = kDeleteEmbeddings.Get();
  rebuild_embeddings = kRebuildEmbeddings.Get();
  use_database_before_embedder = kUseDatabaseBeforeEmbedder.Get();
  use_url_filter = kUseUrlFilter.Get();
  embeddings_service_timeout = kEmbeddingsServiceTimeout.Get();
  enable_side_panel = kEnableSidePanel.Get();
  trim_after_host_in_results = kTrimAfterHostInResults.Get();
  max_answerer_context_url_count = kMaxAnswererContextUrlCount.Get();
  word_match_min_embedding_score = kWordMatchMinEmbeddingScore.Get();
  word_match_min_term_length = kWordMatchMinTermLength.Get();
  word_match_score_boost_factor = kWordMatchScoreBoostFactor.Get();
  word_match_limit = kWordMatchLimit.Get();
  word_match_smoothing_factor = kWordMatchSmoothingFactor.Get();
  word_match_max_term_count = kWordMatchMaxTermCount.Get();
  word_match_required_term_ratio = kWordMatchRequiredTermRatio.Get();
  scroll_tags_enabled = kScrollTagsEnabled.Get();
  erase_non_ascii_characters = kEraseNonAsciiCharacters.Get();
}

FeatureParameters::FeatureParameters(const FeatureParameters&) = default;
FeatureParameters::FeatureParameters(FeatureParameters&&) = default;
FeatureParameters& FeatureParameters::operator=(const FeatureParameters&) =
    default;
FeatureParameters& FeatureParameters::operator=(FeatureParameters&&) = default;

const FeatureParameters& GetFeatureParameters() {
  return GetFeatureParametersMutable();
}

void SetFeatureParametersForTesting(FeatureParameters parameters) {
  GetFeatureParametersMutable() = parameters;
}

}  // namespace history_embeddings
