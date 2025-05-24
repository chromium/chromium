// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"

#include <cmath>
#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autocomplete_scoring_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"

using ModelInput = AutocompleteScoringModelExecutor::ModelInput;
using ModelOutput = AutocompleteScoringModelExecutor::ModelOutput;
using ScoringSignals = ::metrics::OmniboxScoringSignals;
using ::optimization_guide::proto::AutocompleteScoringModelMetadata;
using ::optimization_guide::proto::OptimizationTarget;
using ::optimization_guide::proto::ScoringSignalSpec;
using ::optimization_guide::proto::ScoringSignalTransformation;

constexpr float kDefaultMissingValue = -1;
constexpr float kSecondsInDay = 86400;

namespace {

// Checks if the signal value is valid.
bool IsValidSignal(float val, const ScoringSignalSpec& signal_spec) {
  if (signal_spec.has_min_value() && val < signal_spec.min_value()) {
    return false;
  }
  if (signal_spec.has_max_value() && val > signal_spec.max_value()) {
    return false;
  }
  return true;
}

// Transforms the input signals according to the configured transformation.
float TransformSignal(float val, ScoringSignalTransformation transformation) {
  switch (transformation) {
    case optimization_guide::proto::SCORING_SIGNAL_TRANSFORMATION_LOG_2:
      return log2(val + 1);
    case optimization_guide::proto::SCORING_SIGNAL_TRANSFORMATION_UNKNOWN:
    default:
      DVLOG(0) << "Unknown scoring signal transformation type!";
      return val;
  }
}

}  // namespace

AutocompleteScoringModelHandler::AutocompleteScoringModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    std::unique_ptr<AutocompleteScoringModelExecutor> model_executor,
    OptimizationTarget optimization_target,
    const std::optional<optimization_guide::proto::Any>& model_metadata)
    : optimization_guide::ModelHandler<ModelOutput, ModelInput>(
          model_provider,
          model_executor_task_runner,
          std::move(model_executor),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          model_metadata) {
  // Store the model in memory as soon as it is available and keep it loaded for
  // the whole browser session since model inference is latency sensitive and it
  // cannot wait for the model to be loaded from disk.
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(false);
}

AutocompleteScoringModelHandler::~AutocompleteScoringModelHandler() = default;

std::optional<std::vector<float>>
AutocompleteScoringModelHandler::GetModelInput(
    const ScoringSignals& scoring_signals) {
  DCHECK(ModelAvailable());
  std::optional<AutocompleteScoringModelMetadata> model_metadata =
      ParsedSupportedFeaturesForLoadedModel<AutocompleteScoringModelMetadata>();
  if (!model_metadata) {
    return std::nullopt;
  }

  return ExtractInputFromScoringSignals(scoring_signals,
                                        model_metadata.value());
}

std::optional<std::vector<std::vector<float>>>
AutocompleteScoringModelHandler::GetBatchModelInput(
    const std::vector<const ScoringSignals*>& scoring_signals_vec) {
  std::vector<std::vector<float>> batch_model_input;
  for (const auto* scoring_signals : scoring_signals_vec) {
    std::optional<std::vector<float>> model_input =
        GetModelInput(*scoring_signals);
    if (!model_input) {
      // Return null if any input in the batch is invalid.
      return std::nullopt;
    }
    batch_model_input.push_back(std::move(*model_input));
  }
  return batch_model_input;
}

std::vector<float>
AutocompleteScoringModelHandler::ExtractInputFromScoringSignals(
    const ScoringSignals& scoring_signals,
    const AutocompleteScoringModelMetadata& metadata) {
  // Keep consistent:
  // - omnibox_event.proto `ScoringSignals`
  // - omnibox_scoring_signals.proto `OmniboxScoringSignals`
  // - autocomplete_scoring_model_handler.cc
  //   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
  // - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
  // - autocomplete_controller.cc `RecordScoringSignalCoverageForProvider()`
  // - omnibox_metrics_provider.cc `GetScoringSignalsForLogging()`
  // - omnibox.mojom `struct Signals`
  // - omnibox_page_handler.cc
  //   `TypeConverter<AutocompleteMatch::ScoringSignals, mojom::SignalsPtr>`
  // - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
  //   AutocompleteMatch::ScoringSignals>`
  // - omnibox_util.ts `signalNames`
  // - omnibox/histograms.xml
  //   `Omnibox.URLScoringModelExecuted.ScoringSignalCoverage`

  std::vector<float> model_input;
  for (const auto& scoring_signal_spec : metadata.scoring_signal_specs()) {
    std::optional<float> val;
    switch (scoring_signal_spec.type()) {
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_TYPED_COUNT:
        if (scoring_signals.has_typed_count()) {
          val = static_cast<float>(scoring_signals.typed_count());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_VISIT_COUNT:
        if (scoring_signals.has_visit_count()) {
          val = static_cast<float>(scoring_signals.visit_count());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_VISIT_SECS:
        if (scoring_signals.has_elapsed_time_last_visit_secs()) {
          val = static_cast<float>(
              scoring_signals.elapsed_time_last_visit_secs());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_VISIT_DAYS:
        if (scoring_signals.has_elapsed_time_last_visit_secs()) {
          val = static_cast<float>(
                    scoring_signals.elapsed_time_last_visit_secs()) /
                kSecondsInDay;
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_HOST_ONLY:
        if (scoring_signals.has_is_host_only()) {
          val = static_cast<float>(scoring_signals.is_host_only());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_LENGTH_OF_URL:
        if (scoring_signals.has_length_of_url()) {
          val = static_cast<float>(scoring_signals.length_of_url());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_FIRST_URL_MATCH_POSITION:
        if (scoring_signals.has_first_url_match_position()) {
          val = static_cast<float>(scoring_signals.first_url_match_position());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_HOST_MATCH_AT_WORD_BOUNDARY:
        if (scoring_signals.has_host_match_at_word_boundary()) {
          val =
              static_cast<float>(scoring_signals.host_match_at_word_boundary());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_URL_MATCH_LENGTH:
        if (scoring_signals.has_total_url_match_length()) {
          val = static_cast<float>(scoring_signals.total_url_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_HOST_MATCH_LENGTH:
        if (scoring_signals.has_total_host_match_length()) {
          val = static_cast<float>(scoring_signals.total_host_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_PATH_MATCH_LENGTH:
        if (scoring_signals.has_total_path_match_length()) {
          val = static_cast<float>(scoring_signals.total_path_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_QUERY_OR_REF_MATCH_LENGTH:
        if (scoring_signals.has_total_query_or_ref_match_length()) {
          val = static_cast<float>(
              scoring_signals.total_query_or_ref_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_TITLE_MATCH_LENGTH:
        if (scoring_signals.has_total_title_match_length()) {
          val = static_cast<float>(scoring_signals.total_title_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_NUM_INPUT_TERMS_MATCHED_BY_TITLE:
        if (scoring_signals.has_num_input_terms_matched_by_title()) {
          val = static_cast<float>(
              scoring_signals.num_input_terms_matched_by_title());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_NUM_INPUT_TERMS_MATCHED_BY_URL:
        if (scoring_signals.has_num_input_terms_matched_by_url()) {
          val = static_cast<float>(
              scoring_signals.num_input_terms_matched_by_url());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ALLOWED_TO_BE_DEFAULT_MATCH:
        if (scoring_signals.has_allowed_to_be_default_match()) {
          val =
              static_cast<float>(scoring_signals.allowed_to_be_default_match());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_HAS_NON_SCHEME_WWW_MATCH:
        if (scoring_signals.has_has_non_scheme_www_match()) {
          val = static_cast<float>(scoring_signals.has_non_scheme_www_match());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_TOTAL_BOOKMARK_TITLE_MATCH_LENGTH:
        if (scoring_signals.has_total_bookmark_title_match_length()) {
          val = static_cast<float>(
              scoring_signals.total_bookmark_title_match_length());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_FIRST_BOOKMARK_TITLE_MATCH_POSITION:
        if (scoring_signals.has_first_bookmark_title_match_position()) {
          val = static_cast<float>(
              scoring_signals.first_bookmark_title_match_position());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_NUM_BOOKMARKS_OF_URL:
        if (scoring_signals.has_num_bookmarks_of_url()) {
          val = static_cast<float>(scoring_signals.num_bookmarks_of_url());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_SHORTCUT_VISIT_COUNT:
        if (scoring_signals.has_shortcut_visit_count()) {
          val = static_cast<float>(scoring_signals.shortcut_visit_count());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_SHORTEST_SHORTCUT_LEN:
        if (scoring_signals.has_shortest_shortcut_len()) {
          val = static_cast<float>(scoring_signals.shortest_shortcut_len());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_SHORTCUT_VISIT_SEC:
        if (scoring_signals.has_elapsed_time_last_shortcut_visit_sec()) {
          val = static_cast<float>(
              scoring_signals.elapsed_time_last_shortcut_visit_sec());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_SHORTCUT_VISIT_DAYS:
        if (scoring_signals.has_elapsed_time_last_shortcut_visit_sec()) {
          val = static_cast<float>(
                    scoring_signals.elapsed_time_last_shortcut_visit_sec()) /
                kSecondsInDay;
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_MATCHES_TITLE_OR_HOST_OR_SHORTCUT_TEXT: {
        bool matches_title_or_host_or_shortcut_text = false;
        matches_title_or_host_or_shortcut_text |=
            (scoring_signals.total_host_match_length() > 0);
        matches_title_or_host_or_shortcut_text |=
            (scoring_signals.total_title_match_length() > 0);
        matches_title_or_host_or_shortcut_text |=
            (scoring_signals.shortcut_visit_count() > 0);

        val = static_cast<float>(matches_title_or_host_or_shortcut_text);
      } break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_NUM_INPUT_TERMS_MATCHED_BY_BOOKMARK_TITLE:
        if (scoring_signals.has_num_input_terms_matched_by_bookmark_title()) {
          val = static_cast<float>(
              scoring_signals.num_input_terms_matched_by_bookmark_title());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_SITE_ENGAGEMENT:
        if (scoring_signals.has_site_engagement()) {
          val = static_cast<float>(scoring_signals.site_engagement());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_SEARCH_SUGGEST_RELEVANCE:
        if (scoring_signals.has_search_suggest_relevance()) {
          val = static_cast<float>(scoring_signals.search_suggest_relevance());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_IS_SEARCH_SUGGEST_ENTITY:
        if (scoring_signals.has_is_search_suggest_entity()) {
          val = static_cast<float>(scoring_signals.is_search_suggest_entity());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_VERBATIM:
        if (scoring_signals.has_is_verbatim()) {
          val = static_cast<float>(scoring_signals.is_verbatim());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_NAVSUGGEST:
        if (scoring_signals.has_is_navsuggest()) {
          val = static_cast<float>(scoring_signals.is_navsuggest());
        }
        break;
      case optimization_guide::proto::
          SCORING_SIGNAL_TYPE_IS_SEARCH_SUGGEST_TAIL:
        if (scoring_signals.has_is_search_suggest_tail()) {
          val = static_cast<float>(scoring_signals.is_search_suggest_tail());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_ANSWER_SUGGEST:
        if (scoring_signals.has_is_answer_suggest()) {
          val = static_cast<float>(scoring_signals.is_answer_suggest());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_CALCULATOR_SUGGEST:
        if (scoring_signals.has_is_calculator_suggest()) {
          val = static_cast<float>(scoring_signals.is_calculator_suggest());
        }
        break;
      case optimization_guide::proto::SCORING_SIGNAL_TYPE_UNKNOWN:
      default:
        // Reached when the metadata is updated to have a new signal that
        // the binary hasn't yet been updated to have; or when the binary
        // has updated to remove a previous signal that the metadata hasn't
        // yet been updated to remove.
        DVLOG(0) << "Unknown scoring signal enum type in model metadata!";
        break;
    }

    // Treat invalid signals as missing. Invalid signals may be caused by
    // client errors, e.g., negative elapsed time.
    if (val && !IsValidSignal(*val, scoring_signal_spec)) {
      DVLOG(0) << "Invalid scoring signal value of '"
               << optimization_guide::proto::ScoringSignalType_Name(
                      scoring_signal_spec.type())
               << "': " << *val;
      val = std::nullopt;
    }

    if (val && scoring_signal_spec.has_transformation()) {
      val = TransformSignal(*val, scoring_signal_spec.transformation());
    }

    // Set default value if missing.
    if (!val) {
      val = scoring_signal_spec.has_missing_value()
                ? scoring_signal_spec.missing_value()
                : kDefaultMissingValue;
    }

    // Normalize signal if configured.
    if (scoring_signal_spec.has_norm_upper_boundary()) {
      float upper_boundary = scoring_signal_spec.norm_upper_boundary();
      DCHECK_GT(upper_boundary, 0);
      val = std::clamp(*val, -upper_boundary, upper_boundary) / upper_boundary;
    }

    model_input.push_back(*val);
  }
  DCHECK_EQ(static_cast<size_t>(model_input.size()),
            static_cast<size_t>(metadata.scoring_signal_specs().size()));
  return model_input;
}
