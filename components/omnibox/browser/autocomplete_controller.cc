// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <inttypes.h>
#include <limits.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/bookmark_provider.h"
#include "components/omnibox/browser/bookmark_scoring_signals_annotator.h"
#include "components/omnibox/browser/builtin_provider.h"
#include "components/omnibox/browser/calculator_provider.h"
#include "components/omnibox/browser/clipboard_provider.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/omnibox/browser/history_scoring_signals_annotator.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/local_history_zero_suggest_provider.h"
#include "components/omnibox/browser/most_visited_sites_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/on_device_head_provider.h"
#include "components/omnibox/browser/open_tab_provider.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/search_scoring_signals_annotator.h"
#include "components/omnibox/browser/shortcuts_provider.h"
#include "components/omnibox/browser/url_scoring_signals_annotator.h"
#include "components/omnibox/browser/voice_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "net/http/http_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_canon.h"
#include "url/url_util.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/featured_search_provider.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "components/history_clusters/core/config.h"  // nogncheck
#include "components/history_embeddings/history_embeddings_features.h"  //nogncheck
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/history_cluster_provider.h"
#include "components/omnibox/browser/history_embeddings_provider.h"
#include "components/open_from_clipboard/clipboard_recent_content_generic.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#endif

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

namespace {

using ScoringSignals = ::metrics::OmniboxScoringSignals;
using ProviderType = AutocompleteProvider::Type;

constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);

void RecordMlScoreCoverage(size_t matches_with_non_null_scores,
                           size_t total_scored_matches) {
  int percent_score_coverage =
      matches_with_non_null_scores * 100 / total_scored_matches;
  base::UmaHistogramPercentage(
      "Omnibox.URLScoringModelExecuted.MLScoreCoverage",
      percent_score_coverage);
}

// Records the coverage (i.e. null vs non-null values) for each of the
// `scoring_signals` associated with matches generated by the given `provider`.
void RecordScoringSignalCoverageForProvider(
    const ScoringSignals& scoring_signals,
    const AutocompleteProvider* provider) {
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

  if (!provider) {
    return;
  }

  // Map from each scoring signal type to whether or not it has a non-null
  // value.
  std::vector<std::pair<std::string, bool>> scoring_signal_values{
      {"typed_count", scoring_signals.has_typed_count()},
      {"visit_count", scoring_signals.has_visit_count()},
      {"elapsed_time_last_visit_secs",
       scoring_signals.has_elapsed_time_last_visit_secs()},
      {"shortcut_visit_count", scoring_signals.has_shortcut_visit_count()},
      {"shortest_shortcut_len", scoring_signals.has_shortest_shortcut_len()},
      {"elapsed_time_last_shortcut_visit_sec",
       scoring_signals.has_elapsed_time_last_shortcut_visit_sec()},
      {"is_host_only", scoring_signals.has_is_host_only()},
      {"num_bookmarks_of_url", scoring_signals.has_num_bookmarks_of_url()},
      {"first_bookmark_title_match_position",
       scoring_signals.has_first_bookmark_title_match_position()},
      {"total_bookmark_title_match_length",
       scoring_signals.has_total_bookmark_title_match_length()},
      {"num_input_terms_matched_by_bookmark_title",
       scoring_signals.has_num_input_terms_matched_by_bookmark_title()},
      {"first_url_match_position",
       scoring_signals.has_first_url_match_position()},
      {"total_url_match_length", scoring_signals.has_total_url_match_length()},
      {"host_match_at_word_boundary",
       scoring_signals.has_host_match_at_word_boundary()},
      {"total_host_match_length",
       scoring_signals.has_total_host_match_length()},
      {"total_path_match_length",
       scoring_signals.has_total_path_match_length()},
      {"total_query_or_ref_match_length",
       scoring_signals.has_total_query_or_ref_match_length()},
      {"total_title_match_length",
       scoring_signals.has_total_title_match_length()},
      {"has_non_scheme_www_match",
       scoring_signals.has_has_non_scheme_www_match()},
      {"num_input_terms_matched_by_title",
       scoring_signals.has_num_input_terms_matched_by_title()},
      {"num_input_terms_matched_by_url",
       scoring_signals.has_num_input_terms_matched_by_url()},
      {"length_of_url", scoring_signals.has_length_of_url()},
      {"site_engagement", scoring_signals.has_site_engagement()},
      {"allowed_to_be_default_match",
       scoring_signals.has_allowed_to_be_default_match()},
      {"search_suggest_relevance",
       scoring_signals.has_search_suggest_relevance()},
      {"is_search_suggest_entity",
       scoring_signals.has_is_search_suggest_entity()},
      {"is_verbatim", scoring_signals.has_is_verbatim()},
      {"is_navsuggest", scoring_signals.has_is_navsuggest()},
      {"is_search_suggest_tail", scoring_signals.has_is_search_suggest_tail()},
      {"is_answer_suggest", scoring_signals.has_is_answer_suggest()},
      {"is_calculator_suggest", scoring_signals.has_is_calculator_suggest()},
  };

  const std::string provider_type = provider->GetName();

  for (const auto& pair : scoring_signal_values) {
    base::UmaHistogramBoolean(
        "Omnibox.URLScoringModelExecuted.ScoringSignalCoverage." +
            provider_type + "." + pair.first,
        pair.second);
  }
}

void RecordMlScoringElapsedTime(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Omnibox.URLScoringModelExecuted.ElapsedTime", elapsed,
      base::Microseconds(1), base::Milliseconds(3), 100);
}

void RecordTotalMatchesScored(size_t num_scored) {
  base::UmaHistogramCounts1000("Omnibox.URLScoringModelExecuted.Matches",
                               num_scored);
}

// Appends available autocompletion of the given type, subtype, and number to
// the existing available autocompletions string, encoding according to the
// spec.
std::string ConstructAvailableAutocompletion(
    omnibox::SuggestType type,
    const base::flat_set<omnibox::SuggestSubtype>& subtypes,
    int count) {
  std::ostringstream result;
  result << int(type);

  for (auto subtype : subtypes) {
    result << 'i' << subtype;
  }

  if (count > 1) {
    result << 'l' << count;
  }

  return result.str();
}

// Whether this autocomplete match type supports custom descriptions.
bool AutocompleteMatchHasCustomDescription(const AutocompleteMatch& match) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP &&
      match.type == AutocompleteMatchType::CALCULATOR) {
    return true;
  }
  return match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         match.type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE ||
         match.type == AutocompleteMatchType::CLIPBOARD_TEXT ||
         match.type == AutocompleteMatchType::CLIPBOARD_IMAGE;
}

// Returns which rich autocompletion type, if any, had (or would have had for
// counterfactual variations) an impact; i.e. whether the top scoring rich
// autocompleted suggestion outscores the top scoring default suggestion.
AutocompleteMatch::RichAutocompletionType TopMatchRichAutocompletionType(
    const AutocompleteResult& result) {
  // Trigger rich autocompletion logging if the highest scoring match has
  // |rich_autocompletion_triggered| set indicating it is, or could have been
  // rich autocompleted. It's not sufficient to check the default match since
  // counterfactual variations will not allow rich autocompleted matches to be
  // the default match.
  if (result.empty())
    return AutocompleteMatch::RichAutocompletionType::kNone;

  auto get_sort_key = [](const AutocompleteMatch& match) {
    return std::make_tuple(
        match.allowed_to_be_default_match ||
            match.rich_autocompletion_triggered !=
                AutocompleteMatch::RichAutocompletionType::kNone,
        match.relevance);
  };

  auto top_match = base::ranges::max_element(result, {}, get_sort_key);
  return top_match->rich_autocompletion_triggered;
}

void RecordMatchDeletion(const AutocompleteMatch& match) {
  if (match.deletable) {
    // This formula combines provider and result type into a single enum as
    // defined in OmniboxProviderAndResultType in enums.xml.
    auto combined_type = match.provider->AsOmniboxEventProviderType() * 100 +
                         match.GetOmniboxEventResultType();
    // This histogram is defined in the internal histograms.xml. This is because
    // the vast majority of OmniboxProviderAndResultType histograms are
    // generated by internal tools, and we wish to keep them together.
    base::UmaHistogramSparse("Omnibox.SuggestionDeleted.ProviderAndResultType",
                             combined_type);
  }
}

// Return if the default match from a previous pass should be preserved.
bool ShouldPreserveLastDefaultMatch(
    AutocompleteController::UpdateType update_type,
    const AutocompleteInput& input) {
  // Don't preserve default in keyword mode to avoid e.g. the 'google.com'
  // suggestion being preserved and kicking the user out of keyword mode when
  // they type 'google.com  '.
  if (input.prefer_keyword())
    return false;

  // Preserve for all async updates, but only for longer inputs for sync
  // updates. This mitigates aggressive scoring search suggestions getting
  // 'stuck' as the default when short inputs provide low confidence.
  if (update_type == AutocompleteController::UpdateType::kSyncPassOnly ||
      update_type == AutocompleteController::UpdateType::kSyncPass)
    return input.text().length() >= 4;
  else
    return true;
}

// Helper function to retrieve domains that will be used to find a match between
// historical suggestions and a company entity suggestion. Matches of
// AutocompleteMatchType::HISTORY_URL type will return the domain of
// |destination_url| and those of AutocompleteMatchType::SEARCH_SUGGEST_ENTITY
// will return the domain of |website_uri|. For any other match types,
// GetDomain() should not be called.
std::u16string GetDomain(const AutocompleteMatch& match) {
  DCHECK(match.type == AutocompleteMatchType::HISTORY_URL ||
         match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  GURL url = match.type == AutocompleteMatchType::HISTORY_URL
                 ? match.destination_url
                 : GURL(match.website_uri);
  std::u16string url_host;
  std::u16string url_domain;
  url_formatter::SplitHost(url, &url_host, &url_domain, nullptr);
  return url_domain;
}

std::string EncodeURIComponent(const std::string& component) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(component, &encoded);
  return std::string(encoded.view());
}

}  // namespace

AutocompleteController::OldResult::OldResult(UpdateType update_type,
                                             AutocompleteInput input,
                                             AutocompleteResult* result) {
  if (result->default_match()) {
    last_default_match = *result->default_match();
    if (last_default_match->associated_keyword) {
      last_default_associated_keyword =
          last_default_match->associated_keyword->keyword;
    }
  }

  if (last_default_match &&
      ShouldPreserveLastDefaultMatch(update_type, input)) {
    default_match_to_preserve = last_default_match;
  }

  if (update_type == UpdateType::kSyncPass ||
      update_type == UpdateType::kAsyncPass) {
    matches_to_transfer.SwapMatchesWith(result);
  } else {
    result->ClearMatches();
  }
}

AutocompleteController::OldResult::~OldResult() = default;

// static
std::string AutocompleteController::UpdateTypeToDebugString(
    UpdateType update_type) {
  switch (update_type) {
    case UpdateType::kNone:
      return "None";
    case UpdateType::kSyncPassOnly:
      return "Sync pass only";
    case UpdateType::kSyncPass:
      return "Sync pass";
    case UpdateType::kAsyncPass:
      return "Async pass";
    case UpdateType::kLastAsyncPassExceptDoc:
      return "Last async pass except doc";
    case UpdateType::kExpirePass:
      return "Expire pass";
    case UpdateType::kLastAsyncPass:
      return "Last async pass";
    case UpdateType::kStop:
      return "Stop";
    case UpdateType::kMatchDeletion:
      return "Match deletion";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
void AutocompleteController::ExtendMatchSubtypes(
    const AutocompleteMatch& match,
    base::flat_set<omnibox::SuggestSubtype>* subtypes) {
  // If provider is TYPE_ZERO_SUGGEST_LOCAL_HISTORY, TYPE_ZERO_SUGGEST, or
  // TYPE_ON_DEVICE_HEAD, set the subtype accordingly.
  if (match.provider) {
    if (match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) {
      // Make sure changes here are reflected in UpdateSearchboxStats()
      // below in which the zero-prefix suggestions are counted.
      // We abuse this subtype and use it to for zero-suggest suggestions that
      // aren't personalized by the server. That is, it indicates either
      // client-side most-likely URL suggestions or server-side suggestions
      // that depend only on the URL as context.
      if (match.type == AutocompleteMatchType::NAVSUGGEST) {
        subtypes->emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
        subtypes->emplace(omnibox::SUBTYPE_URL_BASED);
      } else if (match.type == AutocompleteMatchType::SEARCH_SUGGEST) {
        subtypes->emplace(omnibox::SUBTYPE_URL_BASED);
      }
    } else if (match.provider->type() ==
               AutocompleteProvider::TYPE_ON_DEVICE_HEAD) {
      // This subtype indicates a match from an on-device head provider.
      subtypes->emplace(omnibox::SUBTYPE_SUGGEST_2G_LITE);
      // Make sure changes here are reflected in UpdateSearchboxStats()
      // below in which the zero-prefix suggestions are counted.
    } else if (match.provider->type() ==
               AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY) {
      subtypes->emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY);
    }
  }

  switch (match.type) {
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED: {
      subtypes->emplace(omnibox::SUBTYPE_PERSONAL);
      break;
    }
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_ECHO_SEARCH);
      break;
    }
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_ECHO_URL);
      break;
    }
    case AutocompleteMatchType::SEARCH_HISTORY: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_HISTORY_SEARCH);
      break;
    }
    case AutocompleteMatchType::HISTORY_URL: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_HISTORY_URL);
      break;
    }
    case AutocompleteMatchType::HISTORY_TITLE: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_HISTORY_TITLE);
      break;
    }
    case AutocompleteMatchType::HISTORY_BODY: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_HISTORY_BODY);
      break;
    }
    case AutocompleteMatchType::HISTORY_KEYWORD: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_HISTORY_KEYWORD);
      break;
    }
    case AutocompleteMatchType::BOOKMARK_TITLE: {
      subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_BOOKMARK_TITLE);
      break;
    }
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED: {
      subtypes->emplace(omnibox::SUBTYPE_PERSONAL);
      break;
    }
    case AutocompleteMatchType::CLIPBOARD_URL: {
      subtypes->emplace(omnibox::SUBTYPE_CLIPBOARD_URL);
      break;
    }
    case AutocompleteMatchType::CLIPBOARD_TEXT: {
      subtypes->emplace(omnibox::SUBTYPE_CLIPBOARD_TEXT);
      break;
    }
    case AutocompleteMatchType::CLIPBOARD_IMAGE: {
      subtypes->emplace(omnibox::SUBTYPE_CLIPBOARD_IMAGE);
      break;
    }
    default: {
      // This value indicates a native chrome suggestion with no named subtype
      // (yet).
      if (subtypes->empty()) {
        subtypes->emplace(omnibox::SUBTYPE_OMNIBOX_OTHER);
      }
    }
  }
}

// static
int AutocompleteController::ApplyPiecewiseScoringTransform(
    double ml_score,
    std::vector<std::pair<double, int>> break_points) {
  // Start and end points for the line segment whose domain contains `ml_score`.
  std::pair<double, int> start;
  std::pair<double, int> end;
  for (size_t i = 0; i < break_points.size() - 1; i++) {
    start = break_points[i];
    end = break_points[i + 1];
    if (ml_score <= end.first) {
      double m = (end.second - start.second) / (end.first - start.first);
      double b = end.second - m * end.first;
      return m * ml_score + b;
    }
  }
  return 0;
}

AutocompleteController::AutocompleteController(
    std::unique_ptr<AutocompleteProviderClient> provider_client,
    int provider_types,
    bool is_cros_launcher,
    bool disable_ml)
    : provider_client_(std::move(provider_client)),
      bookmark_provider_(nullptr),
      document_provider_(nullptr),
      history_url_provider_(nullptr),
      keyword_provider_(nullptr),
      search_provider_(nullptr),
      zero_suggest_provider_(nullptr),
      on_device_head_provider_(nullptr),
      history_fuzzy_provider_(nullptr),
      notify_changed_debouncer_(false, 200),
      is_cros_launcher_(is_cros_launcher),
      search_service_worker_signal_sent_(false),
      disable_ml_(disable_ml),
      template_url_service_(provider_client_->GetTemplateURLService()),
      triggered_feature_service_(
          provider_client_->GetOmniboxTriggeredFeatureService()),
      steady_state_omnibox_position_(
          metrics::OmniboxEventProto::UNKNOWN_POSITION) {
  provider_types &= ~OmniboxFieldTrial::GetDisabledProviderTypes();

  // Providers run in the order they're added. Async providers should run first
  // so their async requests can be kicked off before waiting a few milliseconds
  // for the other sync providers to complete.
  InitializeAsyncProviders(provider_types);
  InitializeSyncProviders(provider_types);

  // Ideally, we'd check `IsApplicationLocaleSupportedByJourneys()` when
  // constructing `provider_types`. But that's usually constructed in
  // `AutocompleteClassifier::DefaultOmniboxProviders` which can't depend on the
  // browser dir to detect locale. The alternative of piping in the locale from
  // each call site seems too intrusive for a temporary condition (some call
  // sites are also in the components dir). All callers of
  // `DefaultOmniboxProviders` only use it to then construct
  // `AutocompleteController`, so placing the check here instead has no behavior
  // change.
  // TODO(manukh): Move this to `InitializeAsyncProviders()`.
#if !BUILDFLAG(IS_IOS)
  // HistoryClusters is not enabled on iOS.
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER &&
      history_clusters::IsApplicationLocaleSupportedByJourneys(
          provider_client_->GetApplicationLocale()) &&
      search_provider_ != nullptr && history_url_provider_ != nullptr &&
      history_quick_provider_ != nullptr) {
    providers_.push_back(new HistoryClusterProvider(
        provider_client_.get(), this, search_provider_, history_url_provider_,
        history_quick_provider_));
  }
#endif

  // Create URL scoring signal annotators.
  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled() &&
      OmniboxFieldTrial::AreScoringSignalsAnnotatorsEnabled()) {
    url_scoring_signals_annotators_.push_back(
        std::make_unique<HistoryScoringSignalsAnnotator>(
            provider_client_.get()));
    url_scoring_signals_annotators_.push_back(
        std::make_unique<BookmarkScoringSignalsAnnotator>(
            provider_client_.get()));
    url_scoring_signals_annotators_.push_back(
        std::make_unique<UrlScoringSignalsAnnotator>());
    url_scoring_signals_annotators_.push_back(
        std::make_unique<SearchScoringSignalsAnnotator>());
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "AutocompleteController",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

AutocompleteController::~AutocompleteController() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  // The providers may have tasks outstanding that hold refs to them.  We need
  // to ensure they won't call us back if they outlive us.  (Practically,
  // calling Stop() should also cancel those tasks and make it so that we hold
  // the only refs.)  We also don't want to bother notifying anyone of our
  // result changes here, because the notification observer is in the midst of
  // shutdown too, so we don't ask Stop() to clear `internal_result_` (and
  // notify).
  Stop(false);
}

void AutocompleteController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AutocompleteController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AutocompleteController::Start(const AutocompleteInput& input) {
  TRACE_EVENT1("omnibox", "AutocompleteController::Start", "text",
               base::UTF16ToUTF8(input.text()));

  // Providers assume synchronous inputs (`omit_asynchronous_matches() ==
  // true`) are not zero-suggest ones. See crbug.com/1339425.
  DCHECK(!input.omit_asynchronous_matches() || !input.IsZeroSuggest());

  // Use a zero-suggest input as the signal that zero-prefix suggestions could
  // have been shown in the autocomplete session.
  if (input.IsZeroSuggest()) {
    internal_result_.set_zero_prefix_enabled_in_session(true);
  }

  triggered_feature_service_->ResetInput();

  // When input.omit_asynchronous_matches() is true, the AutocompleteController
  // is being used for text classification, which should not notify observers.
  // TODO(manukh): This seems unnecessary; `AutocompleteClassifier` and
  //   `OmniboxController` use separate instances of `AutocompleteController`,
  //   the former doesn't add observers, the latter always uses
  //   `omit_asynchronous_matches()` set to false. Besides, if that weren't the
  //   case, e.g. the classifier did add an observer, then
  //   `AutocompleteController` should respect that, not assume it's a mistake
  //   and silently ignore the observer. Audit all call paths of `::Start()` to
  //   remove this check.
  if (!input.omit_asynchronous_matches()) {
    for (Observer& obs : observers_)
      obs.OnStart(this, input);
  }

  // Must be called before `expire_timer_.Stop()`, modifying `done_`, or
  // modifying `AutocompleteProvider::done_` below. If the previous request has
  // not completed, and therefore has not been logged yet, will log it now.
  // Likewise, if the providers have not completed, and therefore have not been
  // logged yet, will log them now.
  metrics_.OnStart();

  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  When the user presses or releases the ctrl key, the desired_tld
  // changes, and when the user finishes an IME composition, inline autocomplete
  // may no longer be prevented.  In both these cases the text itself hasn't
  // changed since the last query, and some providers can do much less work (and
  // get matches back more quickly).  Taking advantage of this reduces flicker.
  //
  // NOTE: This comes after constructing |input_| above since that construction
  // can change the text string (e.g. by stripping off a leading '?').
  const bool minimal_changes = (input_.text() == input.text()) &&
                               (input_.allow_exact_keyword_match() ==
                                input.allow_exact_keyword_match()) &&
                               (input_.omit_asynchronous_matches() ==
                                input.omit_asynchronous_matches()) &&
                               (input_.focus_type() == input.focus_type());
  input_ = input;

  // Start the new query.
  last_update_type_ = UpdateType::kNone;
  // Use `start_time` rather than `metrics.start_time_` for
  // 'Omnibox.QueryTime2.*'. They differ by 3 μs, which though too small to be
  // distinguished in the ms-scale buckets, is large enough to move the
  // arithmetic mean.
  base::TimeTicks start_time = base::TimeTicks::Now();

  for (const auto& provider : providers_) {
    // Starter Pack engines in keyword mode only run a subset of the providers,
    // so call `ShouldRunProvider()` to determine which ones should run.
    if (!ShouldRunProvider(provider.get())) {
      continue;
    }

    base::TimeTicks provider_start_time = base::TimeTicks::Now();
    provider->Start(input_, minimal_changes);

    // `UmaHistogramTimes()` uses 1ms - 10s buckets, whereas this uses 1ms - 5s
    // buckets.
    // TODO(crbug.com/1340291|manukh): This isn't handled by `metrics_` yet. It
    //   will "automatically" move to `metrics_` if we make all providers async.
    //   Otherwise, if we decide not to make all providers async, move this
    //   there.
    base::TimeTicks provider_end_time = base::TimeTicks::Now();
    base::UmaHistogramCustomTimes(
        std::string("Omnibox.ProviderTime2.") + provider->GetName(),
        provider_end_time - provider_start_time, base::Milliseconds(1),
        base::Seconds(5), 20);
  }
  if (!input.omit_asynchronous_matches()) {
    auto elapsed_time = base::TimeTicks::Now() - start_time;
    // `UmaHistogramTimes()` uses 1ms - 10s buckets, whereas this uses 1ms -
    // 1s buckets.
    // TODO(crbug.com/1340291|manukh): This isn't handled by `metrics_` yet.
    //   Do so after we decide whether to make all providers async.
    base::UmaHistogramCustomTimes("Omnibox.QueryTime2", elapsed_time,
                                  base::Milliseconds(1), base::Seconds(1), 50);
    if (input.text().length() < 6) {
      base::UmaHistogramCustomTimes(
          "Omnibox.QueryTime2." + base::NumberToString(input.text().length()),
          elapsed_time, base::Milliseconds(1), base::Seconds(1), 50);
    }
  }
  base::UmaHistogramBoolean("Omnibox.Start.WantAsyncMatches",
                            !input.omit_asynchronous_matches());

  // `done` will usually be false, unless all providers are finished after the
  // synchronous pass just completed.
  bool done = GetProviderDoneState() == ProviderDoneState::kAllDone;
  DCHECK(!input_.omit_asynchronous_matches() || done);

  UpdateResult(done ? UpdateType::kSyncPassOnly : UpdateType::kSyncPass);

  // If the input looks like a query, send a signal predicting that the user is
  // going to issue a search (either to the default search engine or to a
  // keyword search engine, as indicated by the destination_url). This allows
  // any associated service worker to start up early and reduce the latency of a
  // resulting search. However, to avoid a potentially expensive operation, we
  // only do this once per session. Additionally, a default match is expected to
  // be available at this point but we check anyway to guard against an invalid
  // dereference.
  if (input.type() == metrics::OmniboxInputType::QUERY &&
      !search_service_worker_signal_sent_ && internal_result_.default_match()) {
    search_service_worker_signal_sent_ = true;
    provider_client_->StartServiceWorker(
        internal_result_.default_match()->destination_url);
  }
}

void AutocompleteController::StartPrefetch(const AutocompleteInput& input) {
  TRACE_EVENT1("omnibox", "AutocompleteController::StartPrefetch", "text",
               base::UTF16ToUTF8(input.text()));

  for (auto provider : providers_) {
    if (!ShouldRunProvider(provider.get())) {
      continue;
    }

    // Avoid starting a prefetch request if a non-prefetch request is in
    // progress. Though explicitly discouraged as per documentation in
    // AutocompleteProvider::StartPrefetch(), a provider may still cancel its
    // in-flight non-prefetch request when a prefetch request is started. This
    // may cause the provider to never get a chance to notify the controller of
    // its status; resulting in the controller to remain in an invalid state.
    if (!provider->done()) {
      continue;
    }

    provider->StartPrefetch(input);
    DCHECK(provider->done());
  }
}

void AutocompleteController::Stop(bool clear_result,
                                  bool due_to_user_inactivity) {
  // Must be called before `expire_timer_.Stop()`, modifying `done_`, or
  // modifying `AutocompleteProvider::done_` below. If the current request has
  // not completed, and therefore has not been logged yet, will log it now.
  // Likewise, if the providers have not completed, and therefore have not been
  // logged yet, will log them now.
  metrics_.OnStop();

  for (const auto& provider : providers_) {
    if (!ShouldRunProvider(provider.get()))
      continue;
    provider->Stop(clear_result, due_to_user_inactivity);
  }

  UpdateResult(UpdateType::kStop);

  // Cancel any pending requests that may update the results. Otherwise, e.g.,
  // the user's suggestion selection may be reset.
  CancelNotifyChangedRequest();

  const bool non_empty_result = !internal_result_.empty();
  if (clear_result) {
    internal_result_.Reset();
    if (non_empty_result) {
      // Pass `notify_default_match` as false to clear only the popup and not
      // the edit. Passing true would, e.g., discard the selected suggestion
      // when closing the omnibox.
      RequestNotifyChanged(/*notify_default_match=*/false, /*delayed=*/false);
    }
  }
}

void AutocompleteController::DeleteMatch(const AutocompleteMatch& match) {
  TRACE_EVENT0("omnibox", "AutocompleteController::DeleteMatch");
  DCHECK(match.SupportsDeletion());

  // Delete duplicate matches attached to the main match first.
  for (const auto& duplicate_match : match.duplicate_matches) {
    if (duplicate_match.deletable)
      duplicate_match.provider->DeleteMatch(duplicate_match);
  }

  if (match.deletable) {
    RecordMatchDeletion(match);
    match.provider->DeleteMatch(match);
  }

  // Removes deleted match. Does not re-score URLs so that we don't wait on the
  // posted task, therefore notifying listeners as soon as possible.
  UpdateResult(UpdateType::kMatchDeletion);
}

void AutocompleteController::DeleteMatchElement(const AutocompleteMatch& match,
                                                size_t element_index) {
  DCHECK(match.SupportsDeletion());

  if (match.deletable) {
    RecordMatchDeletion(match);
    match.provider->DeleteMatchElement(match, element_index);
  }

  OnProviderUpdate(true, nullptr);
}

void AutocompleteController::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  TRACE_EVENT0("omnibox", "AutocompleteController::OnProviderUpdate");
  // Should be called even if `sync_pass_done_` is false in order to include
  // early exited async providers. If the provider is done, will log how long
  // the provider took.
  if (provider)
    metrics_.OnProviderUpdate(*provider);

  // Providers should only call this method during the asynchronous pass.
  // There's no reason to call this during the synchronous pass, since we
  // call `UpdateResult()` after the sync pass anyways. This is not a DCHECK,
  // because in the unusual case that a provider calls an asynchronous method,
  // and that method early exits by calling the callback immediately, it's not
  // necessarily a programmer error. We should just no-op.
  if (last_update_type_ == UpdateType::kNone)
    return;

  // Providers shouldn't be running and calling `OnProviderUpdate()` after
  // autocompletion has stopped.
  DCHECK(!done()) << "last_update_type_: "
                  << AutocompleteController::UpdateTypeToDebugString(
                         last_update_type_)
                  << ", provider: "
                  << (provider ? provider->GetName() : "null");

  auto done_state = GetProviderDoneState();

  if (done_state == ProviderDoneState::kAllDone)
    UpdateResult(UpdateType::kLastAsyncPass);
  else if (done_state == ProviderDoneState::kAllExceptDocDone)
    UpdateResult(UpdateType::kLastAsyncPassExceptDoc);
  else if (updated_matches)
    UpdateResult(UpdateType::kAsyncPass);

  if (done_state == ProviderDoneState::kAllDone) {
    size_t calculator_count =
        base::ranges::count_if(published_result_, [](const auto& match) {
          return match.type == AutocompleteMatchType::CALCULATOR;
        });
    UMA_HISTOGRAM_COUNTS_100("Omnibox.NumCalculatorMatches", calculator_count);
  }
}

void AutocompleteController::AddProviderAndTriggeringLogs(
    OmniboxLog* logs) const {
  TRACE_EVENT0("omnibox",
               "AutocompleteController::AddProviderAndTriggeringLogs");
  logs->providers_info.clear();
  for (const auto& provider : providers_) {
    if (!ShouldRunProvider(provider.get()))
      continue;

    // Add per-provider info, if any.
    provider->AddProviderInfo(&logs->providers_info);

    // This is also a good place to put code to add info that you want to
    // add for every provider.
  }

  logs->steady_state_omnibox_position = steady_state_omnibox_position_;

  // Add any features that have been triggered.
  triggered_feature_service_->RecordToLogs(
      &logs->features_triggered, &logs->features_triggered_in_session);
}

void AutocompleteController::ResetSession() {
  search_service_worker_signal_sent_ = false;
  triggered_feature_service_->ResetSession();
}

void AutocompleteController::
    UpdateMatchDestinationURLWithAdditionalSearchboxStats(
        base::TimeDelta query_formulation_time,
        AutocompleteMatch* match) const {
  TRACE_EVENT0("omnibox",
               "AutocompleteController::"
               "UpdateMatchDestinationURLWithAdditionalSearchboxStats");
  // The searchbox_stats is expected to have been previously set when this
  // method is called. If that is not the case, this method is being called by
  // mistake and searchbox_stats should not be updated with additional
  // information.
  if (!match->search_terms_args ||
      match->search_terms_args->searchbox_stats.ByteSizeLong() == 0) {
    return;
  }

  UpdateSearchTermsArgsWithAdditionalSearchboxStats(query_formulation_time,
                                                    *match->search_terms_args);
  SetMatchDestinationURL(match);
}

void AutocompleteController::UpdateSearchTermsArgsWithAdditionalSearchboxStats(
    base::TimeDelta query_formulation_time,
    TemplateURLRef::SearchTermsArgs& search_terms_args) const {
  // Append the query formulation time (time from when the user first typed a
  // character into the omnibox to when the user selected a query), whether
  // a field trial has triggered, and the current page classification to the
  // searchbox stats parameter.
  bool search_feature_triggered =
      triggered_feature_service_->GetFeatureTriggeredInSession(
          metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE) ||
      triggered_feature_service_->GetFeatureTriggeredInSession(
          metrics::OmniboxEventProto_Feature_REMOTE_ZERO_SUGGEST_FEATURE);
  const std::string experiment_stats = base::StringPrintf(
      "%" PRId64 "j%dj%d", query_formulation_time.InMilliseconds(),
      search_feature_triggered, input_.current_page_classification());
  // TODO(crbug.com/40197024): experiment_stats is a deprecated field. We should
  // however continue to report it for the downstream consumers that expect this
  // field. Eventually Chrome should start logging the substitute fields and
  // the downstream consumers should migrate to using those fields before we
  // can stop logging this deprecated field.
  search_terms_args.searchbox_stats.set_experiment_stats(experiment_stats);

  // Append the ExperimentStatsV2 to the searchbox stats parameter to be logged
  // in searchbox_stats.proto's experiment_stats_v2 field.
  if (zero_suggest_provider_) {
    for (const auto& experiment_stat_v2 :
         zero_suggest_provider_->experiment_stats_v2s()) {
      // The string value consists of suggestion type/subtype pairs delimited
      // with colons. However, the SearchboxStats logging flow expects
      // suggestion type/subtype pairs to be delimited with commas instead.
      std::string value = experiment_stat_v2.string_value();
      std::replace(value.begin(), value.end(), ':', ',');
      auto* reported_experiment_stats_v2 =
          search_terms_args.searchbox_stats.add_experiment_stats_v2();
      reported_experiment_stats_v2->set_type_int(experiment_stat_v2.type_int());
      reported_experiment_stats_v2->set_string_value(value);
    }
  }
#if BUILDFLAG(IS_IOS)
  // Append the omnibox position when it's set to experiment_stats_v2.
  if (steady_state_omnibox_position_ !=
      metrics::OmniboxEventProto::UNKNOWN_POSITION) {
    const auto omnibox_position_stat = GetOmniboxPositionExperimentStatsV2();
    auto* reported_experiment_stats_v2 =
        search_terms_args.searchbox_stats.add_experiment_stats_v2();
    reported_experiment_stats_v2->set_type_int(
        omnibox_position_stat.type_int());
    reported_experiment_stats_v2->set_int_value(
        omnibox_position_stat.int_value());
  }
#endif
}

void AutocompleteController::SetMatchDestinationURL(
    AutocompleteMatch* match) const {
  TRACE_EVENT0("omnibox", "AutocompleteController::SetMatchDestinationURL");

  // Convert search terms to UTF8 and URI-component encode the string.
  const std::string encoded_search_terms = EncodeURIComponent(
      base::UTF16ToUTF8(match->search_terms_args->search_terms));

  // Append an extra header to navigations from the @gemini scope.
  const TemplateURL* turl = match->GetTemplateURL(template_url_service_, false);
  if (turl && turl->starter_pack_id() == TemplateURLStarterPackData::kGemini &&
      !encoded_search_terms.empty() &&
      net::HttpUtil::IsValidHeaderValue(encoded_search_terms)) {
    DCHECK(net::HttpUtil::IsValidHeaderName(kOmniboxGeminiHeader));
    match->extra_headers =
        base::StrCat({kOmniboxGeminiHeader, ":", encoded_search_terms});
  }

  auto url = ComputeURLFromSearchTermsArgs(turl, *match->search_terms_args);
  if (url.is_valid()) {
    match->destination_url = std::move(url);
  }
#if BUILDFLAG(IS_ANDROID)
  match->UpdateJavaDestinationUrl();
#endif
}

void AutocompleteController::GroupSuggestionsBySearchVsURL(size_t begin,
                                                           size_t end) {
  if (begin == end)
    return;
  TRACE_EVENT0("omnibox",
               "AutocompleteController::GroupSuggestionsBySearchVsURL");
  AutocompleteResult& result = const_cast<AutocompleteResult&>(this->result());
  const size_t num_elements = result.size();
  if (begin < 0 || end <= begin || end > num_elements) {
    DCHECK(false) << "Range [" << begin << "; " << end
                  << ") is not valid for grouping; accepted range: [0; "
                  << num_elements << ").";
    return;
  }

  AutocompleteResult::GroupSuggestionsBySearchVsURL(
      std::next(result.begin(), begin), std::next(result.begin(), end));
}

bool AutocompleteController::ShouldRunProvider(
    AutocompleteProvider* provider) const {
  if (!provider) {
    return false;
  }

  // Only a subset of providers are run for the Lens searchboxes.
  if (omnibox::IsLensSearchbox(input_.current_page_classification())) {
    return provider->type() == AutocompleteProvider::TYPE_SEARCH ||
           provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST;
  }

  if (omnibox::IsAndroidHub(input_.current_page_classification())) {
    return provider->type() == AutocompleteProvider::TYPE_SEARCH ||
           provider->type() == AutocompleteProvider::TYPE_OPEN_TAB;
  }

  if (input_.InKeywordMode()) {
    // Only a subset of providers are run when we're in a starter pack keyword
    // mode. Try to grab the TemplateURL to determine if we're in starter pack
    // mode and whether this provider should be run.
    AutocompleteInput keyword_input = input_;
    const TemplateURL* keyword_turl =
        KeywordProvider::GetSubstitutingTemplateURLForInput(
            template_url_service_, &keyword_input);

    if (keyword_turl && keyword_turl->starter_pack_id() > 0) {
      switch (provider->type()) {
        // Keyword provider creates the suggestion attached to the keyword chip
        // and search provider creates the SEARCH_OTHER_ENGINE suggestion
        // required for keyword mode to work. These still need to be run or
        // keyword mode breaks.
        // search-what-you-typed suggestions from the DSE are usually provided
        // by the search provider, but are skipped within the search provider
        // logic when in keyword mode, so do not need to be handled here..
        case AutocompleteProvider::TYPE_SEARCH:
        case AutocompleteProvider::TYPE_KEYWORD:
          return true;

        // @Bookmarks starter pack scope - run only the bookmarks provider.
        case AutocompleteProvider::TYPE_BOOKMARK:
          return (keyword_turl->starter_pack_id() ==
                  TemplateURLStarterPackData::kBookmarks);

        // @History starter pack scope - run the history providers & featured
        // search for embeddings IPH suggestions.
        case AutocompleteProvider::TYPE_HISTORY_QUICK:
        case AutocompleteProvider::TYPE_HISTORY_URL:
        case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
        case AutocompleteProvider::TYPE_FEATURED_SEARCH:
          return (keyword_turl->starter_pack_id() ==
                  TemplateURLStarterPackData::kHistory);

        // @Tabs starter pack scope - run the open tab provider.
        case AutocompleteProvider::TYPE_OPEN_TAB:
          return (keyword_turl->starter_pack_id() ==
                  TemplateURLStarterPackData::kTabs);

        // No other providers should run when in a starter pack scope.
        default:
          return false;
      }
    }

    // Outside of the starter pack scopes, keyword mode should still restrict
    // certain providers.
    switch (provider->type()) {
      // Don't run history cluster provider or on device head provider.
      case AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER:
      case AutocompleteProvider::TYPE_ON_DEVICE_HEAD:
        return false;

      // Don't run document provider, except for Google Drive.
      case AutocompleteProvider::TYPE_DOCUMENT:
        return (keyword_turl &&
                base::StartsWith(keyword_turl->url(),
                                 "https://drive.google.com",
                                 base::CompareCase::INSENSITIVE_ASCII));

      // Treat all other providers as usual.
      default:
        break;
    }
  }

  // Some providers should only run in starter pack mode or in the CrOS
  // launcher. If we reach here, we're not in starter pack mode.
  switch (provider->type()) {
    case AutocompleteProvider::TYPE_OPEN_TAB:
      return is_cros_launcher_;
#if !BUILDFLAG(IS_IOS)
    case AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS:
      return history_embeddings::kOmniboxUnscoped.Get();
#endif
    default:
      break;
  }

  // Otherwise, run all providers.
  return true;
}

GURL AutocompleteController::ComputeURLFromSearchTermsArgs(
    const TemplateURL* template_url,
    const TemplateURLRef::SearchTermsArgs& search_terms_args) const {
  if (!template_url) {
    return GURL();
  }

  // Skip search term replacement when in the @gemini scope.
  // TODO(crbug.com/41494524): Replace this logic with a proper fix to support
  // keywords that do not do search term replacement in omnibox.
  if (template_url->starter_pack_id() == TemplateURLStarterPackData::kGemini) {
    return GURL(OmniboxFieldTrial::kGeminiUrlOverride.Get());
  }

  return GURL(template_url->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service_->search_terms_data()));
}

void AutocompleteController::InitializeAsyncProviders(int provider_types) {
  if (provider_types & AutocompleteProvider::TYPE_SEARCH) {
    search_provider_ = new SearchProvider(provider_client_.get(), this);
    providers_.push_back(search_provider_.get());
  }
  // Providers run in the order they're added.  Add `HistoryURLProvider` after
  // `SearchProvider` because:
  // - `SearchProvider` synchronously queries the history database's
  //   keyword_search_terms and url table.
  // - `HistoryUrlProvider` schedules a background task that also accesses the
  //    history database.
  // If both db accesses happen concurrently, TSan complains. So put
  // `HistoryURLProvider` later to make sure that `SearchProvider` is done
  // doing its thing by the time the `HistoryURLProvider` task runs. (And hope
  // that it completes before `AutocompleteController::Start()` is called the
  // next time.)
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_URL) {
    history_url_provider_ =
        new HistoryURLProvider(provider_client_.get(), this);
    providers_.push_back(history_url_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_DOCUMENT) {
    document_provider_ = DocumentProvider::Create(provider_client_.get(), this);
    providers_.push_back(document_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_ON_DEVICE_HEAD) {
    on_device_head_provider_ =
        OnDeviceHeadProvider::Create(provider_client_.get(), this);
    providers_.push_back(on_device_head_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_CALCULATOR &&
      search_provider_ != nullptr) {
    providers_.push_back(
        new CalculatorProvider(provider_client_.get(), this, search_provider_));
  }
#if !BUILDFLAG(IS_IOS)
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS) {
    providers_.push_back(
        new HistoryEmbeddingsProvider(provider_client_.get(), this));
  }
#endif
}

void AutocompleteController::InitializeSyncProviders(int provider_types) {
  if (provider_types & AutocompleteProvider::TYPE_BOOKMARK) {
    bookmark_provider_ = new BookmarkProvider(provider_client_.get());
    providers_.push_back(bookmark_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_BUILTIN)
    providers_.push_back(new BuiltinProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_QUICK) {
    history_quick_provider_ = new HistoryQuickProvider(provider_client_.get());
    providers_.push_back(history_quick_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_KEYWORD) {
    keyword_provider_ = new KeywordProvider(provider_client_.get(), this);
    providers_.push_back(keyword_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_SHORTCUTS)
    providers_.push_back(new ShortcutsProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_ZERO_SUGGEST) {
    zero_suggest_provider_ =
        ZeroSuggestProvider::Create(provider_client_.get(), this);
    if (zero_suggest_provider_)
      providers_.push_back(zero_suggest_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY) {
    providers_.push_back(
        LocalHistoryZeroSuggestProvider::Create(provider_client_.get(), this));
  }
  if (provider_types & AutocompleteProvider::TYPE_MOST_VISITED_SITES) {
    providers_.push_back(
        new MostVisitedSitesProvider(provider_client_.get(), this));
    // Note: the need for the always-present verbatim match originates from the
    // search-ready omnibox (SRO) in Incognito mode, where the
    // ZeroSuggestProvider intentionally never gets invoked.
    providers_.push_back(
        new ZeroSuggestVerbatimMatchProvider(provider_client_.get()));
  }
  if (provider_types & AutocompleteProvider::TYPE_CLIPBOARD) {
#if !BUILDFLAG(IS_IOS)
    // On iOS, a global ClipboardRecentContent should've been created by now
    // (if enabled).  If none has been created (e.g., we're on a different
    // platform), use the generic implementation, which AutocompleteController
    // will own.  Don't try to create a generic implementation on iOS because
    // iOS doesn't want/need to link in the implementation and the libraries
    // that would come with it.
    if (!ClipboardRecentContent::GetInstance()) {
      ClipboardRecentContent::SetInstance(
          std::make_unique<ClipboardRecentContentGeneric>());
    }
#endif
    // ClipboardRecentContent can be null in iOS tests.  For non-iOS, we
    // create a ClipboardRecentContent as above (for both Chrome and tests).
    if (ClipboardRecentContent::GetInstance()) {
      clipboard_provider_ = new ClipboardProvider(
          provider_client_.get(), this, ClipboardRecentContent::GetInstance());
      providers_.push_back(clipboard_provider_.get());
    }
  }
  if (provider_types & AutocompleteProvider::TYPE_VOICE_SUGGEST) {
    voice_suggest_provider_ = new VoiceSuggestProvider(provider_client_.get());
    providers_.push_back(voice_suggest_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_FUZZY) {
    history_fuzzy_provider_ = new HistoryFuzzyProvider(provider_client_.get());
    providers_.push_back(history_fuzzy_provider_.get());
  }
  if (provider_types & AutocompleteProvider::TYPE_OPEN_TAB) {
    open_tab_provider_ = new OpenTabProvider(provider_client_.get());
    providers_.push_back(open_tab_provider_.get());
  }
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (provider_types & AutocompleteProvider::TYPE_FEATURED_SEARCH) {
    featured_search_provider_ =
        new FeaturedSearchProvider(provider_client_.get());
    providers_.push_back(featured_search_provider_.get());
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

void AutocompleteController::UpdateResult(UpdateType update_type) {
  TRACE_EVENT0("omnibox", "AutocompleteController::UpdateResult");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Omnibox.AutocompletionTime.UpdateResult");

#if DCHECK_IS_ON()
  auto debug_string =
      AutocompleteController::UpdateTypeToDebugString(last_update_type_) +
      " -> " + AutocompleteController::UpdateTypeToDebugString(update_type);

  switch (update_type) {
    case UpdateType::kSyncPassOnly:
    case UpdateType::kSyncPass:
      DCHECK(last_update_type_ == UpdateType::kNone) << debug_string;
      break;

    case UpdateType::kAsyncPass:
    case UpdateType::kLastAsyncPassExceptDoc:
      DCHECK(last_update_type_ == UpdateType::kSyncPass ||
             last_update_type_ == UpdateType::kAsyncPass ||
             last_update_type_ == UpdateType::kExpirePass)
          << debug_string;
      break;

    case UpdateType::kExpirePass:
      DCHECK(last_update_type_ == UpdateType::kSyncPass ||
             last_update_type_ == UpdateType::kLastAsyncPassExceptDoc ||
             last_update_type_ == UpdateType::kAsyncPass)
          << debug_string;
      break;

    case UpdateType::kLastAsyncPass:
      DCHECK(last_update_type_ == UpdateType::kSyncPass ||
             last_update_type_ == UpdateType::kAsyncPass ||
             last_update_type_ == UpdateType::kLastAsyncPassExceptDoc ||
             last_update_type_ == UpdateType::kExpirePass)
          << debug_string;
      break;

    case UpdateType::kMatchDeletion:
      DCHECK(last_update_type_ != UpdateType::kNone) << debug_string;
      break;

    case UpdateType::kStop:
      // All cases are valid.
      break;

    case UpdateType::kNone:
      NOTREACHED_IN_MIGRATION();
  }
#endif  // DCHECK_IS_ON()

  last_update_type_ = update_type;

  if (update_type == UpdateType::kSyncPassOnly ||
      update_type == UpdateType::kSyncPass ||
      update_type == UpdateType::kLastAsyncPass ||
      update_type == UpdateType::kStop) {
    expire_timer_.Stop();
    stop_timer_.Stop();
  }

  if (update_type == UpdateType::kStop)
    return;

  OldResult old_result(update_type, input_, &internal_result_);
  AggregateNewMatches();

  MlRerank(old_result);

  if (update_type == UpdateType::kSyncPass ||
      update_type == UpdateType::kAsyncPass ||
      update_type == UpdateType::kLastAsyncPassExceptDoc) {
    internal_result_.SortAndCull(input_, template_url_service_,
                                 triggered_feature_service_,
                                 old_result.default_match_to_preserve);
    internal_result_.TransferOldMatches(input_,
                                        &old_result.matches_to_transfer);
  }

  internal_result_.SortAndCull(input_, template_url_service_,
                               triggered_feature_service_,
                               old_result.default_match_to_preserve);

  if (update_type == UpdateType::kSyncPass) {
    StartExpireTimer();
    StartStopTimer();
  }

  PostProcessMatches();

  bool default_match_changed = CheckWhetherDefaultMatchChanged(
      old_result.last_default_match,
      old_result.last_default_associated_keyword);

  // Pretend the default match changed for sync passes, because when the user
  // types a character, the inline autocompletion selection must be updated
  // even if the current match has the same URL as the last run's default match.
  // Likewise, the controller doesn't know what's happened in the edit since the
  // last time it ran autocomplete. The user might have selected all the text
  // and hit delete, then typed a new character. The selection and delete won't
  // send any signals to the controller so it doesn't realize that anything was
  // cleared or changed. Even if the default match hasn't changed, we need the
  // edit model to update the display.
  default_match_changed = default_match_changed ||
                          update_type == UpdateType::kSyncPassOnly ||
                          update_type == UpdateType::kSyncPass;

  bool immediate = update_type == UpdateType::kSyncPassOnly ||
                   update_type == UpdateType::kSyncPass ||
                   update_type == UpdateType::kLastAsyncPass ||
                   update_type == UpdateType::kMatchDeletion ||
                   (omnibox_feature_configs::DocumentProvider::Get()
                        .ignore_when_debouncing &&
                    update_type == UpdateType::kLastAsyncPassExceptDoc);

  RequestNotifyChanged(default_match_changed, !immediate);
}

void AutocompleteController::AggregateNewMatches() {
  for (const auto& provider : providers_) {
    if (!ShouldRunProvider(provider.get()))
      continue;

    // Append the new matches and conditionally set a swap bit. This logic
    // was previously within `AppendMatches` but here is the only place
    // where it's still needed, and even this should ideally be cleaned up.
    size_t match_index = internal_result_.size();
    internal_result_.AppendMatches(provider->matches());
    for (; match_index < internal_result_.size(); match_index++) {
      AutocompleteMatch* match = internal_result_.match_at(match_index);
      if (!match->description.empty() &&
          !AutocompleteMatch::IsSearchType(match->type) &&
          match->type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
        match->swap_contents_and_description = true;
      }

      if (omnibox_feature_configs::ForceAllowedToBeDefault::Get().enabled &&
          !match->allowed_to_be_default_match && match->keyword.empty() &&
          !input_.prevent_inline_autocomplete()) {
        match->allowed_to_be_default_match = true;
        match->RecordAdditionalInfo("force allowed to be default", "true");
      }
    }

    internal_result_.MergeSuggestionGroupsMap(
        provider->suggestion_groups_map());
  }
}

void AutocompleteController::MlRerank(OldResult& old_result) {
  // Annotate the eligible matches in `internal_result_` with additional scoring
  // signals. The additional signals in `internal_result_` will be lost when
  // `UpdateResult()` is called again. Currently, `internal_result_` is updated
  // in each `UpdateResult()` call.
  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled() &&
      OmniboxFieldTrial::AreScoringSignalsAnnotatorsEnabled()) {
    for (const auto& annotator : url_scoring_signals_annotators_) {
      annotator->AnnotateResult(input_, &internal_result_);
    }
  }

  if (internal_result_.empty())
    return;
  if (!OmniboxFieldTrial::IsMlUrlScoringEnabled())
    return;
  if (!provider_client_->GetAutocompleteScoringModelService())
    return;
  if (disable_ml_)
    return;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (OmniboxFieldTrial::GetMLConfig().piecewise_mapped_search_blending) {
    RunBatchUrlScoringModelPiecewiseMappedSearchBlending(old_result);
  } else if (OmniboxFieldTrial::GetMLConfig().mapped_search_blending) {
    RunBatchUrlScoringModelMappedSearchBlending(old_result);
  } else {
    RunBatchUrlScoringModel(old_result);
  }
#else
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

void AutocompleteController::PostProcessMatches() {
#if DCHECK_IS_ON()
  internal_result_.Validate();
#endif  // DCHECK_IS_ON()

  AttachActions();
  UpdateKeywordDescriptions(&internal_result_);
  UpdateAssociatedKeywords(&internal_result_);
  UpdateSearchboxStats(&internal_result_);
  UpdateTailSuggestPrefix(&internal_result_);
  MaybeRemoveCompanyEntityImages(&internal_result_);
  MaybeCleanSuggestionsForKeywordMode(input_, &internal_result_);

  // Notify providers which of their matches were shown. If we end up with more
  // providers to notify, we should add `RegisterDisplayedMatches()` to the
  // `AutocompleteProvider` interface and iterate all providers here.
  if (search_provider_)
    search_provider_->RegisterDisplayedAnswers(internal_result_);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // `featured_search_provider_` isn't interested in "invisible" autocomplete
  // runs, e.g. when text is copied.
  if (featured_search_provider_ && !input_.omit_asynchronous_matches())
    featured_search_provider_->RegisterDisplayedMatches(internal_result_);
#endif

  // Mark the rich autocompletion feature triggered if the top match, or
  // would-be-top-match if rich autocompletion is counterfactual enabled, is
  // rich autocompleted.
  const auto top_match_rich_autocompletion_type =
      TopMatchRichAutocompletionType(internal_result_);
  triggered_feature_service_->RichAutocompletionTypeTriggered(
      top_match_rich_autocompletion_type);
  if (top_match_rich_autocompletion_type !=
      AutocompleteMatch::RichAutocompletionType::kNone) {
    triggered_feature_service_->FeatureTriggered(
        metrics::OmniboxEventProto_Feature_RICH_AUTOCOMPLETION);
  }
}

bool AutocompleteController::CheckWhetherDefaultMatchChanged(
    std::optional<AutocompleteMatch> last_default_match,
    std::u16string last_default_associated_keyword) {
  const bool default_is_valid = internal_result_.default_match();
  std::u16string default_associated_keyword;
  if (default_is_valid &&
      internal_result_.default_match()->associated_keyword) {
    default_associated_keyword =
        internal_result_.default_match()->associated_keyword->keyword;
  }
  // We've gotten async results. Send notification that the default match
  // updated if fill_into_edit, associated_keyword, or keyword differ.  (The
  // second can change if we've just started Chrome and the keyword database
  // finishes loading while processing this request.  The third can change
  // if we swapped from interpreting the input as a search--which gets
  // labeled with the default search provider's keyword--to a URL.)
  // We don't check the URL as that may change for the default match
  // even though the fill into edit hasn't changed (see SearchProvider
  // for one case of this).
  const bool notify_default_match =
      (last_default_match.has_value() != default_is_valid) ||
      (last_default_match &&
       ((internal_result_.default_match()->fill_into_edit !=
         last_default_match->fill_into_edit) ||
        (default_associated_keyword != last_default_associated_keyword) ||
        (internal_result_.default_match()->keyword !=
         last_default_match->keyword)));
  if (notify_default_match)
    last_time_default_match_changed_ = base::TimeTicks::Now();
  return notify_default_match;
}

void AutocompleteController::AttachActions() {
  if (!input_.IsZeroSuggest()) {
    // Do not look for matching tabs on Android unless we collected all the
    // suggestions. Tab matching is an expensive process with multiple JNI calls
    // involved. Run it only when all the suggestions are collected.
    bool perform_tab_match = is_android ? done() : true;
    if (perform_tab_match) {
      internal_result_.ConvertOpenTabMatches(provider_client_.get(), &input_);
    }

    // Do not attach pedals to matches in the Lens Searchbox.
    if (!omnibox::IsLensSearchbox(input_.current_page_classification())) {
      internal_result_.AttachPedalsToMatches(input_, *provider_client_);
    }

#if !BUILDFLAG(IS_IOS)
    // HistoryClusters is not enabled on iOS.
    AttachHistoryClustersActions(provider_client_->GetHistoryClustersService(),
                                 internal_result_);
#endif
  }
  internal_result_.TrimOmniboxActions(input_.IsZeroSuggest());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  internal_result_.SplitActionsToSuggestions();
#endif
}

void AutocompleteController::UpdateAssociatedKeywords(
    AutocompleteResult* result) {
  if (!keyword_provider_)
    return;

  // Determine if the user's input is an exact keyword match.
  std::u16string exact_keyword =
      keyword_provider_->GetKeywordForText(input_.text());

  std::set<std::u16string> keywords;
  for (AutocompleteMatch& match : *result) {
    std::u16string keyword(
        match.GetSubstitutingExplicitlyInvokedKeyword(template_url_service_));
    if (!keyword.empty()) {
      keywords.insert(keyword);
      continue;
    }

    // When the user has typed an exact keyword, we want tab-to-search on the
    // default match to select that keyword, even if the match
    // inline-autocompletes to a different keyword.  (This prevents inline
    // autocompletions from blocking a user's attempts to use an explicitly-set
    // keyword of their own creation.)  So use |exact_keyword| if it's
    // available.
    if (!exact_keyword.empty() && !keywords.count(exact_keyword)) {
      // Prevent starter-pack keywords from attaching to non-starter-pack
      // matches. Those will have a dedicated UI with an explicit match
      // selection to enter keyword mode.
      if (kIsDesktop && match.type != AutocompleteMatchType::STARTER_PACK) {
        TemplateURL* turl =
            template_url_service_->GetTemplateURLForKeyword(exact_keyword);
        // Note, starter pack matches that removed the '@' from the beginning of
        // the keyword are still allowed to attach because those don't get the
        // special UX, by design.
        if (turl && turl->starter_pack_id() != 0 &&
            turl->keyword().starts_with(u'@')) {
          continue;
        }
      }

      keywords.insert(exact_keyword);
      // If the match has an answer, it will look strange to try to display
      // it along with a keyword hint. Prefer the keyword hint, and revert
      // to a typical search.
      match.answer.reset();
      match.answer_template.reset();
      match.answer_type = omnibox::ANSWER_TYPE_UNSPECIFIED;
      match.associated_keyword = std::make_unique<AutocompleteMatch>(
          keyword_provider_->CreateVerbatimMatch(exact_keyword, exact_keyword,
                                                 input_));
#if BUILDFLAG(IS_ANDROID)
      match.UpdateJavaAnswer();
#endif
      continue;
    }

    // Otherwise, set a match's associated keyword based on the match's
    // fill_into_edit, which should take inline autocompletions into account.
    keyword = keyword_provider_->GetKeywordForText(match.fill_into_edit);

    if (!keyword.empty()) {
      // Prevent starter-pack keywords from attaching to non-starter-pack
      // matches.
      if (kIsDesktop && match.type != AutocompleteMatchType::STARTER_PACK) {
        TemplateURL* turl =
            template_url_service_->GetTemplateURLForKeyword(keyword);
        if (turl && turl->starter_pack_id() != 0 &&
            turl->keyword().starts_with(u'@')) {
          continue;
        }
      }

      // Only add the keyword if the match does not have a duplicate keyword
      // with a more relevant match.
      if (!keywords.count(keyword) ||
          (kIsDesktop && match.type == AutocompleteMatchType::STARTER_PACK)) {
        keywords.insert(keyword);
        match.associated_keyword = std::make_unique<AutocompleteMatch>(
            keyword_provider_->CreateVerbatimMatch(match.fill_into_edit,
                                                   keyword, input_));
      } else {
        match.associated_keyword.reset();
      }
    }
  }
}

void AutocompleteController::UpdateKeywordDescriptions(
    AutocompleteResult* result) {
  // The Lens searchbox does not require the search engine name description
  // label since all suggestions will be from a single source.
  // TODO(crbug.com/338094774): Remove this Lens-specific change and implement a
  // general solution.
  if (omnibox::IsLensSearchbox(input_.current_page_classification())) {
    return;
  }

  std::u16string last_keyword;
  for (auto i(result->begin()); i != result->end(); ++i) {
    if (AutocompleteMatch::IsSearchType(i->type)) {
      if (AutocompleteMatchHasCustomDescription(*i))
        continue;
      i->description.clear();
      i->description_class.clear();
      DCHECK(!i->keyword.empty());
      if (i->keyword != last_keyword) {
        const TemplateURL* template_url =
            i->GetTemplateURL(template_url_service_, false);
        if (template_url) {
          // For extension keywords, just make the description the extension
          // name -- don't assume that the normal search keyword description
          // is applicable.
          i->description = template_url->AdjustedShortNameForLocaleDirection();
          if (template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION) {
            i->description = l10n_util::GetStringFUTF16(
                IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION, i->description);
          }
          i->description_class.push_back(
              ACMatchClassification(0, ACMatchClassification::DIM));
        }
#if BUILDFLAG(IS_ANDROID)
        i->UpdateJavaDescription();
#endif

        last_keyword = i->keyword;
      }
    } else {
      last_keyword.clear();
    }
  }
}

void AutocompleteController::UpdateSearchboxStats(AutocompleteResult* result) {
  using omnibox::metrics::ChromeSearchboxStats;

  if (result->empty())
    return;

  ChromeSearchboxStats searchbox_stats;
  searchbox_stats.set_client_name("chrome");

  int count = 0;
  int num_zero_prefix_suggestions_shown = 0;
  std::optional<omnibox::SuggestType> last_type;
  base::flat_set<omnibox::SuggestSubtype> last_subtypes = {};
  omnibox::GroupId previous_group_id = omnibox::GROUP_INVALID;
  std::vector<size_t> match_index_to_position(result->size());
  size_t match_position = 0;

  std::vector<bool> match_index_belongs_to_horizontal_render_group(
      result->size());
  std::vector<size_t> match_index_to_aqs_slot(result->size());
  std::vector<std::string> aqs;
  aqs.reserve(result->size());

  for (size_t index = 0; index < result->size(); ++index) {
    AutocompleteMatch* match = result->match_at(index);

    // Consider all AutocompleteMatches belonging to a Horizontal render group
    // as a single element. If suggestion group ID has not changed, and the
    // group render type is horizontal, we won't create a separate entry for
    // this suggestion.
    omnibox::GroupId group_id =
        match->suggestion_group_id.value_or(omnibox::GROUP_INVALID);
    omnibox::GroupConfig_RenderType render_type =
        result->GetRenderTypeForSuggestionGroup(group_id);
    bool match_belongs_to_horizontal_render_group =
        render_type == omnibox::GroupConfig_RenderType_HORIZONTAL;
    match_index_belongs_to_horizontal_render_group[index] =
        match_belongs_to_horizontal_render_group;
    if (group_id == previous_group_id &&
        match_belongs_to_horizontal_render_group) {
      // All elements in a Horizontal Render Group share the same index
      // and AQS slot.
      match_index_to_position[index] = match_position - 1;
      match_index_to_aqs_slot[index] = aqs.size();
      continue;
    }
    previous_group_id = group_id;

    omnibox::SuggestType type = match->suggest_type;
    auto subtypes = match->subtypes;
    ExtendMatchSubtypes(*match, &subtypes);

    if (input_.IsZeroSuggest()) {
      // Count the zero-prefix suggestions in the result set.
      if (subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY) ||
          subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS) ||
          subtypes.contains(
              omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES) ||
          subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX) ||
          subtypes.contains(omnibox::SUBTYPE_CLIPBOARD_IMAGE) ||
          subtypes.contains(omnibox::SUBTYPE_CLIPBOARD_TEXT) ||
          subtypes.contains(omnibox::SUBTYPE_CLIPBOARD_URL)) {
        num_zero_prefix_suggestions_shown++;
      }
    }

    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(match_position);
    available_suggestion->set_type(type);
    match_index_to_position[index] = match_position;
    for (const auto subtype : subtypes) {
      available_suggestion->add_subtypes(subtype);
    }

    if (last_type.has_value() &&
        (type != last_type || subtypes != last_subtypes)) {
      aqs.push_back(
          ConstructAvailableAutocompletion(*last_type, last_subtypes, count));
      count = 1;
    } else {
      count++;
    }
    match_index_to_aqs_slot[index] = aqs.size();
    last_type = type;
    last_subtypes = subtypes;
    match_position++;
  }
  if (last_type.has_value()) {
    aqs.push_back(
        ConstructAvailableAutocompletion(*last_type, last_subtypes, count));
  }

  // If zero-prefix suggestions are offered multiple times, log the most recent
  // count.
  if (num_zero_prefix_suggestions_shown > 0) {
    result->set_num_zero_prefix_suggestions_shown_in_session(
        num_zero_prefix_suggestions_shown);
  }
  searchbox_stats.set_num_zero_prefix_suggestions_shown(
      omnibox_feature_configs::ReportNumZPSInSession::Get().enabled
          ? result->num_zero_prefix_suggestions_shown_in_session()
          : num_zero_prefix_suggestions_shown);
  searchbox_stats.set_zero_prefix_enabled(
      omnibox_feature_configs::ReportNumZPSInSession::Get().enabled
          ? result->zero_prefix_enabled_in_session()
          : searchbox_stats.num_zero_prefix_suggestions_shown() > 0);

  // Go over all matches and set searchbox stats if the match supports it.
  for (size_t index = 0; index < result->size(); ++index) {
    AutocompleteMatch* match = result->match_at(index);
    const TemplateURL* template_url =
        match->GetTemplateURL(template_url_service_, false);
    if (!template_url || !match->search_terms_args)
      continue;

    match->search_terms_args->searchbox_stats = searchbox_stats;

    // Prevent trivial suggestions from getting credit for being selected.
    if (!match->IsTrivialAutocompletion()) {
      match_position = match_index_to_position[index];
      DCHECK_LT(static_cast<int>(match_position),
                match->search_terms_args->searchbox_stats
                    .available_suggestions_size());
      auto* selected_suggestion =
          match->search_terms_args->searchbox_stats
              .mutable_available_suggestions(match_position);
      DCHECK_EQ(static_cast<int>(match_position), selected_suggestion->index());
      selected_suggestion->set_type(match->suggest_type);
      match->search_terms_args->searchbox_stats.mutable_assisted_query_info()
          ->MergeFrom(*selected_suggestion);

      // Reconstruct AQS for items sharing the slot (e.g. elements in the
      // carousel).
      if (match_index_belongs_to_horizontal_render_group[index]) {
        aqs[match_index_to_aqs_slot[index]] = ConstructAvailableAutocompletion(
            match->suggest_type, match->subtypes, 1);
      }
    }

    // Duplicate searchbox stats for eligible ActionsInSuggest.
    // TODO(crbug.com/40257536): rather than computing the `action_uri`, keep
    // the updated search_terms_args, and apply the query formulation time the
    // moment the action is selected.
    for (auto& scoped_action : match->actions) {
      auto* action_in_suggest =
          OmniboxActionInSuggest::FromAction(scoped_action.get());
      auto* answer_action =
          OmniboxAnswerAction::FromAction(scoped_action.get());

      TemplateURLRef::SearchTermsArgs* search_terms_args;
      if (action_in_suggest == nullptr ||
          !action_in_suggest->search_terms_args.has_value()) {
        if (answer_action == nullptr) {
          continue;
        }
        search_terms_args = &answer_action->search_terms_args;
      } else {
        search_terms_args = &action_in_suggest->search_terms_args.value();
      }

      search_terms_args->searchbox_stats.MergeFrom(
          match->search_terms_args->searchbox_stats);

      if (action_in_suggest != nullptr) {
        action_in_suggest->action_info.set_action_uri(
            ComputeURLFromSearchTermsArgs(
                match->GetTemplateURL(template_url_service_, false),
                *search_terms_args)
                .spec());
      }
    }
  }
}

void AutocompleteController::UpdateTailSuggestPrefix(
    AutocompleteResult* result) {
  const auto common_prefix = result->GetCommonPrefix();
  if (!common_prefix.empty()) {
    for (auto& match : *result) {
      if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
        match.tail_suggest_common_prefix = common_prefix;
    }
  }
}

void AutocompleteController::NotifyChanged() {
  TRACE_EVENT0("omnibox", "AutocompleteController::NotifyChanged");
  // Will log metrics for how many matches changed. Will also log timing metrics
  // for the current request if it's complete; otherwise, will just update
  // timestamps of when the last update changed any or the default suggestion.
  metrics_.OnNotifyChanged(last_result_for_logging_,
                           internal_result_.GetMatchDedupComparators());

  // Swap matches from `internal_result_` to `published_result_` and copy them
  // back from `published_result_` to `internal_result_`. This allows
  // `published_result_` to retain `java_match_` and the computed
  // `matching_java_tab_` which otherwise would have been lost if
  // `internal_result_` simply copied matches from `internal_result_`.
  published_result_.SwapMatchesWith(&internal_result_);
  internal_result_.CopyMatchesFrom(published_result_);

  last_result_for_logging_ = internal_result_.GetMatchDedupComparators();

  for (Observer& obs : observers_)
    obs.OnResultChanged(this, notify_changed_default_match_);
  CancelNotifyChangedRequest();
}

void AutocompleteController::RequestNotifyChanged(bool notify_default_match,
                                                  bool delayed) {
  if (notify_default_match)
    notify_changed_default_match_ = true;
  notify_changed_debouncer_.RequestRun(base::BindOnce(
      &AutocompleteController::NotifyChanged, base::Unretained(this)));
  if (!delayed)
    notify_changed_debouncer_.FlushRequest();
}

void AutocompleteController::CancelNotifyChangedRequest() {
  notify_changed_debouncer_.CancelRequest();
  notify_changed_default_match_ = false;
}

AutocompleteController::ProviderDoneState
AutocompleteController::GetProviderDoneState() {
  bool doc_not_done = false;
  for (const auto& provider : providers_) {
    if (!ShouldRunProvider(provider.get()) || provider->done())
      continue;
    if (provider->type() != AutocompleteProvider::TYPE_DOCUMENT)
      return ProviderDoneState::kNotDone;
    else
      doc_not_done = true;
  }
  return doc_not_done ? ProviderDoneState::kAllExceptDocDone
                      : ProviderDoneState::kAllDone;
}

void AutocompleteController::StartExpireTimer() {
  // Amount of time (in ms) between when the user stops typing and
  // when we remove any copied entries. We do this from the time the
  // user stopped typing as some providers (such as SearchProvider)
  // wait for the user to stop typing before they initiate a query.
  const int kExpireTimeMS = 500;

  if (internal_result_.HasCopiedMatches())
    expire_timer_.Start(
        FROM_HERE, base::Milliseconds(kExpireTimeMS),
        base::BindOnce(&AutocompleteController::UpdateResult,
                       base::Unretained(this), UpdateType::kExpirePass));
}

void AutocompleteController::StartStopTimer() {
  stop_timer_.Start(FROM_HERE, stop_timer_duration_,
                    base::BindOnce(&AutocompleteController::Stop,
                                   base::Unretained(this), false, true));
}

bool AutocompleteController::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  size_t res = 0;

  // provider_client_ seems to be small enough to ignore it.

  // TODO(dyaroshev): implement memory estimation for scoped_refptr in
  // base::trace_event.
  res += std::accumulate(providers_.begin(), providers_.end(), 0u,
                         [](size_t sum, const auto& provider) {
                           return sum + sizeof(AutocompleteProvider) +
                                  provider->EstimateMemoryUsage();
                         });

  res += input_.EstimateMemoryUsage();
  res += internal_result_.EstimateMemoryUsage();

  auto* dump = process_memory_dump->CreateAllocatorDump(
      base::StringPrintf("omnibox/autocomplete_controller/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, res);
  return true;
}

void AutocompleteController::SetStartStopTimerDurationForTesting(
    base::TimeDelta duration) {
  stop_timer_duration_ = duration;
}

size_t AutocompleteController::InjectAdHocMatch(AutocompleteMatch match) {
  size_t index = internal_result_.size();
  // Append the match exactly as it is provided, with no change to
  // `swap_contents_and_description`.
  internal_result_.AppendMatches({std::move(match)});
  RequestNotifyChanged(false, false);
  return index;
}

void AutocompleteController::SetSteadyStateOmniboxPosition(
    metrics::OmniboxEventProto::OmniboxPosition position) {
  steady_state_omnibox_position_ = position;
}

const omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2
AutocompleteController::GetOmniboxPositionExperimentStatsV2() const {
  // Field number of the omnibox position in
  // SearchboxStats::ExperimentStatsV2::StatType.
  constexpr int kOmniboxPositionFieldNumber = 95;
  // Value of the enum in SearchboxStats::OmniboxPosition.
  constexpr int kTopOmniboxValue = 1;
  constexpr int kBottomOmniboxValue = 2;

  omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2 experiment_stats_v2;
  experiment_stats_v2.set_type_int(kOmniboxPositionFieldNumber);
  switch (steady_state_omnibox_position_) {
    case metrics::OmniboxEventProto::TOP_POSITION:
      experiment_stats_v2.set_int_value(kTopOmniboxValue);
      break;
    case metrics::OmniboxEventProto::BOTTOM_POSITION:
      experiment_stats_v2.set_int_value(kBottomOmniboxValue);
      break;
    default:
      break;
  }
  return experiment_stats_v2;
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void AutocompleteController::RunBatchUrlScoringModel(OldResult& old_result) {
  TRACE_EVENT0("omnibox", "AutocompleteController::RunBatchUrlScoringModel");

  // Dedupe matches; otherwise, e.g., duplicate bookmark and history matches
  // would be scored independently with their partial signals.
  internal_result_.DeduplicateMatches(input_, template_url_service_);

  size_t eligible_matches_count = base::ranges::count_if(
      internal_result_.matches_,
      [](const auto& match) { return match.IsMlScoringEligible(); });

  if (eligible_matches_count == 0)
    return;

  // Run the model for the eligible matches. Keep a reference to those matches
  // to later redistribute their relevance scores based on the model output.
  std::vector<const ScoringSignals*> batch_scoring_signals;
  batch_scoring_signals.reserve(eligible_matches_count);
  std::vector<ACMatches::iterator> eligible_match_itrs;
  for (auto match_itr = internal_result_.begin();
       match_itr != internal_result_.end(); ++match_itr) {
    if (!match_itr->IsMlScoringEligible()) {
      continue;
    }

    RecordScoringSignalCoverageForProvider(match_itr->scoring_signals.value(),
                                           match_itr->provider.get());

    batch_scoring_signals.push_back(&match_itr->scoring_signals.value());
    eligible_match_itrs.push_back(match_itr);
  }

  auto elapsed_timer = base::ElapsedTimer();
  const auto ml_scores = provider_client_->GetAutocompleteScoringModelService()
                             ->BatchScoreAutocompleteUrlMatchesSync(
                                 std::move(batch_scoring_signals));
  if (ml_scores.empty()) {
    return;
  }

  if (ml_scores.size() != eligible_match_itrs.size()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Record how many eligible matches the model was executed for.
  RecordTotalMatchesScored(ml_scores.size());

  // Record how long it took to execute the model for all eligible matches.
  RecordMlScoringElapsedTime(elapsed_timer.Elapsed());

  // Record whether the model was executed for at least one eligible match.
  provider_client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
      metrics::OmniboxEventProto_Feature_ML_URL_SCORING);

  // The goal is to redistribute the existing relevance scores among the
  // eligible matches according to the model prediction scores.
  // `relevance_heap` is a max heap containing the (legacy) relevance scores,
  // while `prediction_and_match_itr_heap` is a max heap containing tuples of
  // the form (ml_score, legacy_score, match_itr). If two matches have the same
  // ML score (e.g. two remote document suggestions w/o local scoring signals),
  // then the legacy score will be used to break ties.
  std::priority_queue<int> relevance_heap;
  std::priority_queue<std::tuple<float, int, AutocompleteResult::iterator>>
      prediction_and_match_itr_heap;
  size_t score_coverage_count = 0;
  // Likewise, keep the same number of shortcut boosted suggestions but reassign
  // them to the highest scoring suggestions.
  size_t boosted_shortcut_count = 0;
  for (size_t index = 0; index < ml_scores.size(); index++) {
    const auto& prediction = ml_scores[index];
    if (!prediction.has_value()) {
      continue;
    }
    score_coverage_count++;

    auto match_itr = eligible_match_itrs[index];
    relevance_heap.emplace(match_itr->relevance);
    prediction_and_match_itr_heap.emplace(prediction.value(),
                                          match_itr->relevance, match_itr);
    if (match_itr->shortcut_boosted)
      boosted_shortcut_count++;
  }

  // Record the percentage of matches that were assigned non-null scores by
  // the ML scoring model.
  RecordMlScoreCoverage(score_coverage_count, ml_scores.size());

  while (!relevance_heap.empty()) {
    // If not in the counterfactual treatment, assign the highest relevance
    // score to the match with the highest respective model prediction score.
    if (!OmniboxFieldTrial::IsMlUrlScoringCounterfactual()) {
      auto match_itr = std::get<2>(prediction_and_match_itr_heap.top());
      match_itr->RecordAdditionalInfo("ml legacy relevance",
                                      match_itr->relevance);
      match_itr->RecordAdditionalInfo(
          "ml model output", std::get<0>(prediction_and_match_itr_heap.top()));
      match_itr->relevance = relevance_heap.top();
      if (boosted_shortcut_count) {
        match_itr->RecordAdditionalInfo("ML shortcut boosted", "true");
        match_itr->shortcut_boosted = true;
        boosted_shortcut_count--;
      } else {
        match_itr->shortcut_boosted = false;
      }
    }
    relevance_heap.pop();
    prediction_and_match_itr_heap.pop();
  }

  for (Observer& obs : observers_)
    obs.OnMlScored(this, internal_result_);
}

void AutocompleteController::RunBatchUrlScoringModelMappedSearchBlending(
    OldResult& old_result) {
  TRACE_EVENT0(
      "omnibox",
      "AutocompleteController::RunBatchUrlScoringModelMappedSearchBlending");

  // Sort according to traditional scores.
  // This is needed in order to ensure that the relevance score assignment logic
  // can properly break ties when two (or more) URL suggestions have the same ML
  // score.
  internal_result_.Sort(input_, template_url_service_,
                        old_result.default_match_to_preserve);

  // Run the model for the eligible matches.
  std::vector<const ScoringSignals*> batch_scoring_signals;
  std::vector<size_t> scored_positions;
  for (size_t i = 0; i < internal_result_.size(); ++i) {
    const auto& match = internal_result_.matches_[i];
    if (!match.IsMlScoringEligible()) {
      continue;
    }

    RecordScoringSignalCoverageForProvider(match.scoring_signals.value(),
                                           match.provider.get());

    batch_scoring_signals.push_back(&match.scoring_signals.value());
    scored_positions.push_back(i);
  }

  if (batch_scoring_signals.empty()) {
    return;
  }

  auto elapsed_timer = base::ElapsedTimer();
  const auto ml_scores = provider_client_->GetAutocompleteScoringModelService()
                             ->BatchScoreAutocompleteUrlMatchesSync(
                                 std::move(batch_scoring_signals));
  if (ml_scores.empty()) {
    return;
  }

  if (ml_scores.size() != scored_positions.size()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Record how many eligible matches the model was executed for.
  RecordTotalMatchesScored(ml_scores.size());

  // Record how long it took to execute the model for all eligible matches.
  RecordMlScoringElapsedTime(elapsed_timer.Elapsed());

  // Record whether the model was executed for at least one eligible match.
  provider_client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
      metrics::OmniboxEventProto_Feature_ML_URL_SCORING);

  if (OmniboxFieldTrial::IsMlUrlScoringCounterfactual()) {
    return;
  }

  const int min = OmniboxFieldTrial::GetMLConfig().mapped_search_blending_min;
  const int max = OmniboxFieldTrial::GetMLConfig().mapped_search_blending_max;
  const int grouping_threshold = OmniboxFieldTrial::GetMLConfig()
                                     .mapped_search_blending_grouping_threshold;

  size_t score_coverage_count = 0;
  for (size_t i = 0; i < ml_scores.size(); ++i) {
    const auto& prediction = ml_scores[i];
    float p_value = prediction.value_or(0);
    if (prediction.has_value()) {
      score_coverage_count++;
    }
    auto& match = internal_result_.matches_[scored_positions[i]];
    match.RecordAdditionalInfo("ml legacy relevance", match.relevance);
    match.RecordAdditionalInfo("ml model output", p_value);
    match.relevance = min + p_value * (max - min);
    match.shortcut_boosted = match.relevance > grouping_threshold;
  }

  // Record the percentage of matches that were assigned non-null scores by
  // the ML scoring model.
  RecordMlScoreCoverage(score_coverage_count, ml_scores.size());

  // Following the initial relevance assignment, build a sorted list of
  // values which will contain the finalized set of relevance scores for URL
  // suggestions.
  std::vector<int> scores_pool;
  for (size_t i = 0; i < internal_result_.size(); ++i) {
    const auto& match = internal_result_.matches_[i];
    if (!match.IsMlScoringEligible()) {
      continue;
    }
    scores_pool.push_back(match.relevance);
  }
  base::ranges::sort(scores_pool, std::greater<>());

  // Avoid duplicate scores by ensuring that no two URL suggestions are assigned
  // the same score.
  int max_score = INT_MAX;
  for (auto& score : scores_pool) {
    score = std::min(score, max_score - 1);
    max_score = score;
  }

  std::vector<std::pair<float, size_t>> prediction_and_position_heap;
  for (size_t i = 0; i < ml_scores.size(); ++i) {
    const auto& prediction = ml_scores[i];
    prediction_and_position_heap.push_back(
        {prediction.value_or(0), scored_positions[i]});
  }
  base::ranges::stable_sort(prediction_and_position_heap, std::greater<>(),
                            [](const auto& pair) { return pair.first; });

  // Assign the finalized relevance scores to each URL suggestion in order of
  // priority (i.e. ML score).
  for (size_t i = 0; i < prediction_and_position_heap.size(); ++i) {
    auto& match =
        internal_result_.matches_[prediction_and_position_heap[i].second];
    match.relevance = scores_pool[i];
  }

  for (Observer& obs : observers_) {
    obs.OnMlScored(this, internal_result_);
  }
}

void AutocompleteController::
    RunBatchUrlScoringModelPiecewiseMappedSearchBlending(
        OldResult& old_result) {
  TRACE_EVENT0("omnibox",
               "AutocompleteController::"
               "RunBatchUrlScoringModelPiecewiseMappedSearchBlending");

  using PiecewiseMappingVariant = OmniboxFieldTrial::PiecewiseMappingVariant;

  const auto break_points = OmniboxFieldTrial::GetPiecewiseMappingBreakPoints();
  if (break_points.empty()) {
    return;
  }

  const auto break_points_verbatim_url =
      OmniboxFieldTrial::GetPiecewiseMappingBreakPoints(
          PiecewiseMappingVariant::kVerbatimUrl);
  const auto break_points_search =
      OmniboxFieldTrial::GetPiecewiseMappingBreakPoints(
          PiecewiseMappingVariant::kSearch);

  // Sort according to traditional scores.
  // This is needed in order to ensure that the relevance score assignment logic
  // can properly break ties when two (or more) URL suggestions have the same ML
  // score.
  internal_result_.Sort(input_, template_url_service_,
                        old_result.default_match_to_preserve);

  // Run the model for the eligible matches.
  std::vector<const ScoringSignals*> batch_scoring_signals;
  std::vector<size_t> scored_positions;
  for (size_t i = 0; i < internal_result_.size(); ++i) {
    const auto& match = internal_result_.matches_[i];
    if (!match.IsMlScoringEligible()) {
      continue;
    }

    RecordScoringSignalCoverageForProvider(match.scoring_signals.value(),
                                           match.provider.get());

    batch_scoring_signals.push_back(&match.scoring_signals.value());
    scored_positions.push_back(i);
  }

  if (batch_scoring_signals.empty()) {
    return;
  }

  auto elapsed_timer = base::ElapsedTimer();
  const auto ml_scores = provider_client_->GetAutocompleteScoringModelService()
                             ->BatchScoreAutocompleteUrlMatchesSync(
                                 std::move(batch_scoring_signals));
  if (ml_scores.empty()) {
    return;
  }

  if (ml_scores.size() != scored_positions.size()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Record how many eligible matches the model was executed for.
  RecordTotalMatchesScored(ml_scores.size());

  // Record how long it took to execute the model for all eligible matches.
  RecordMlScoringElapsedTime(elapsed_timer.Elapsed());

  // Record whether the model was executed for at least one eligible match.
  provider_client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
      metrics::OmniboxEventProto_Feature_ML_URL_SCORING);

  if (OmniboxFieldTrial::IsMlUrlScoringCounterfactual()) {
    return;
  }

  const int grouping_threshold =
      OmniboxFieldTrial::GetMLConfig()
          .piecewise_mapped_search_blending_grouping_threshold;
  const int relevance_bias =
      OmniboxFieldTrial::GetMLConfig()
          .piecewise_mapped_search_blending_relevance_bias;

  size_t score_coverage_count = 0;
  for (size_t i = 0; i < ml_scores.size(); ++i) {
    const auto& prediction = ml_scores[i];
    float p_value = prediction.value_or(0);
    if (prediction.has_value()) {
      score_coverage_count++;
    }
    auto& match = internal_result_.matches_[scored_positions[i]];
    match.RecordAdditionalInfo("ml legacy relevance", match.relevance);
    match.RecordAdditionalInfo("ml model output", p_value);

    const auto* break_points_for_transform = &break_points;
    if (match.IsVerbatimUrlSuggestion() && !break_points_verbatim_url.empty()) {
      break_points_for_transform = &break_points_verbatim_url;
    } else if (AutocompleteMatch::IsSearchType(match.type) &&
               !break_points_search.empty()) {
      break_points_for_transform = &break_points_search;
    }
    match.relevance =
        ApplyPiecewiseScoringTransform(p_value, *break_points_for_transform) +
        relevance_bias;

    // Shortcut boosting should only be applied to shortcut suggestions that
    // have been used (visited) more than once. This logic will also overwrite
    // whatever value was originally set by ShortcutsProvider for the
    // `shortcut_boosted` property.
    const bool is_shortcut = match.provider && match.provider->type() ==
                                                   ProviderType::TYPE_SHORTCUTS;
    const bool has_enough_visits = match.scoring_signals.has_value() &&
                                   match.scoring_signals->has_visit_count() &&
                                   match.scoring_signals->visit_count() >= 2;
    match.shortcut_boosted = is_shortcut && has_enough_visits &&
                             match.relevance > grouping_threshold;
  }

  // Record the percentage of matches that were assigned non-null scores by
  // the ML scoring model.
  RecordMlScoreCoverage(score_coverage_count, ml_scores.size());

  // Following the initial relevance assignment, build a sorted list of
  // values which will contain the finalized set of relevance scores for URL
  // suggestions.
  std::vector<int> scores_pool;
  for (size_t i = 0; i < internal_result_.size(); ++i) {
    const auto& match = internal_result_.matches_[i];
    if (!match.IsMlScoringEligible()) {
      continue;
    }
    scores_pool.push_back(match.relevance);
  }
  base::ranges::sort(scores_pool, std::greater<>());

  // Avoid duplicate scores by ensuring that no two URL suggestions are assigned
  // the same score.
  int max_score = INT_MAX;
  for (auto& score : scores_pool) {
    score = std::min(score, max_score - 1);
    max_score = score;
  }

  std::vector<std::pair<float, size_t>> prediction_and_position_heap;
  for (size_t i = 0; i < ml_scores.size(); ++i) {
    const auto& prediction = ml_scores[i];
    prediction_and_position_heap.push_back(
        {prediction.value_or(0), scored_positions[i]});
  }
  base::ranges::stable_sort(prediction_and_position_heap, std::greater<>(),
                            [](const auto& pair) { return pair.first; });

  // Assign the finalized relevance scores to each URL suggestion in order of
  // priority (i.e. ML score).
  for (size_t i = 0; i < prediction_and_position_heap.size(); ++i) {
    auto& match =
        internal_result_.matches_[prediction_and_position_heap[i].second];
    match.relevance = scores_pool[i];
  }

  for (Observer& obs : observers_)
    obs.OnMlScored(this, internal_result_);
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void AutocompleteController::MaybeRemoveCompanyEntityImages(
    AutocompleteResult* result) {
  if (result->size() == 0) {
    return;
  }
  std::u16string history_domain;
  // First match must be of history URL type to ablate entity image.
  if (result->match_at(0)->type == AutocompleteMatchType::HISTORY_URL) {
    history_domain = GetDomain(*result->match_at(0));
  }

  auto iter = base::ranges::find_if(result->matches_, [](const auto& match) {
    return match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
  });
  if (iter == result->matches_.end()) {
    return;
  }

  bool image_ablated = false;
  if (!history_domain.empty()) {
    for (auto it = iter; it != result->matches_.end(); it++) {
      // Do not attempt to change image to search loupe if not an entity
      // suggestion.
      if (it->type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
        continue;
      }
      // Check that the entity domain matches the history domain.
      if (history_domain == GetDomain(*it)) {
        it->image_url = GURL();
        it->image_dominant_color.clear();
        image_ablated = true;
      }
    }
  }
  base::UmaHistogramBoolean("Omnibox.CompanyEntityImageAblated", image_ablated);
}

void AutocompleteController::MaybeCleanSuggestionsForKeywordMode(
    const AutocompleteInput& input,
    AutocompleteResult* result) {
  if (input.current_page_classification() ==
      metrics::OmniboxEventProto::NTP_REALBOX) {
    // Realbox doesn't support keyword mode yet, so keep original list intact.
    return;
  }
  if (kIsDesktop && input.text().starts_with(u'@')) {
    // When the input is '@' exactly, some special filtering rules are applied.
    // Note: the rule preserving other matches with `associated_keyword` is
    // not currently necessary, but is intended to make it easy to coexist
    // with enterprise configured scopes when that feature is implemented.
    if (input.text() == u"@") {
      result->EraseMatchesWhere([](const AutocompleteMatch& match) {
        return !(match.type == AutocompleteMatchType::STARTER_PACK ||
                 match.contents == u"@" || match.associated_keyword);
      });
      // Simple sort is needed to restore verbatim '@' search as top/default
      // match because a different default, e.g. "@hill", might have previously
      // occupied the top spot while '@' was demoted below others.
      std::sort(result->begin(), result->end(),
                AutocompleteMatch::MoreRelevant);
      // Put first defaultable match in top position since relevance
      // ranking alone doesn't guarantee it.
      auto default_match = std::find_if(
          result->begin(), result->end(),
          [](const auto& m) { return m.allowed_to_be_default_match; });
      if (default_match != result->begin() && default_match != result->end()) {
        std::rotate(result->begin(), default_match, default_match + 1);
      }
    }

    // Intentionally avoid actions and remove button on first suggestion
    // which may interfere with keyword mode refresh.
    if (result->size() > 1 &&
        AutocompleteMatch::IsFeaturedSearchType(result->match_at(1)->type)) {
      result->match_at(0)->actions.clear();
      result->match_at(0)->deletable = false;
      for (AutocompleteMatch& duplicate :
           result->match_at(0)->duplicate_matches) {
        duplicate.deletable = false;
      }
    }

    // Eliminate tab switch on instant keyword matches for clean appearance.
    for (size_t i = 0; i < result->size(); i++) {
      if (result->match_at(i)->HasInstantKeyword(template_url_service_)) {
        result->match_at(i)->actions.clear();
      }
    }
  }
}
