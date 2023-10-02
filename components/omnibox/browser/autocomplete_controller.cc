// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <inttypes.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/history_clusters/core/config.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
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
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/on_device_head_provider.h"
#include "components/omnibox/browser/open_tab_provider.h"
#include "components/omnibox/browser/query_tile_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/shortcuts_provider.h"
#include "components/omnibox/browser/url_scoring_signals_annotator.h"
#include "components/omnibox/browser/voice_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/history_cluster_provider.h"
#include "components/open_from_clipboard/clipboard_recent_content_generic.h"
#endif

namespace {

using ScoringSignals = ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;

constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);

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
bool ShouldPreserveLastDefaultMatch(bool sync_pass_done,
                                    const AutocompleteInput& input) {
  // Don't preserve default in keyword mode to avoid e.g. the 'google.com'
  // suggestion being preserved and kicking the user out of keyword mode when
  // they type 'google.com  '.
  if (input.prefer_keyword())
    return false;

  // Preserve for all async updates, but only for longer inputs for sync
  // updates. This mitigates aggressive scoring search suggestions getting
  // 'stuck' as the default when short inputs provide low confidence.
  if (!sync_pass_done)
    return input.text().length() >= 4;
  else
    return true;
}

}  // namespace

// static
void AutocompleteController::ExtendMatchSubtypes(
    const AutocompleteMatch& match,
    base::flat_set<omnibox::SuggestSubtype>* subtypes) {
  // If provider is TYPE_ZERO_SUGGEST_LOCAL_HISTORY, TYPE_ZERO_SUGGEST, or
  // TYPE_ON_DEVICE_HEAD, set the subtype accordingly.
  if (match.provider) {
    if (match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) {
      // Make sure changes here are reflected in UpdateAssistedQueryStats()
      // below in which the zero-prefix suggestions are counted.
      // We abuse this subtype and use it to for zero-suggest suggestions that
      // aren't personalized by the server. That is, it indicates either
      // client-side most-likely URL suggestions or server-side suggestions
      // that depend only on the URL as context.
      if (match.type == AutocompleteMatchType::TILE_NAVSUGGEST ||
          match.type == AutocompleteMatchType::TILE_MOST_VISITED_SITE ||
          match.type == AutocompleteMatchType::NAVSUGGEST) {
        subtypes->emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
        subtypes->emplace(omnibox::SUBTYPE_URL_BASED);
      } else if (match.type == AutocompleteMatchType::SEARCH_SUGGEST) {
        subtypes->emplace(omnibox::SUBTYPE_URL_BASED);
      } else if (match.type == AutocompleteMatchType::TILE_REPEATABLE_QUERY) {
        subtypes->emplace(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES);
      }
    } else if (match.provider->type() ==
               AutocompleteProvider::TYPE_ON_DEVICE_HEAD) {
      // This subtype indicates a match from an on-device head provider.
      subtypes->emplace(omnibox::SUBTYPE_SUGGEST_2G_LITE);
      // Make sure changes here are reflected in UpdateAssistedQueryStats()
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

AutocompleteController::AutocompleteController(
    std::unique_ptr<AutocompleteProviderClient> provider_client,
    int provider_types,
    bool is_cros_launcher)
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
  internal_result_.Reset();  // Not really necessary.
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
  // true`) have default focus type (`focus_type() == INTERACTION_DEFAULT`). See
  // crbug.com/1339425.
  DCHECK(!input.omit_asynchronous_matches() ||
         input.focus_type() == metrics::OmniboxFocusType::INTERACTION_DEFAULT);

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

  const std::u16string old_input_text(input_.text());
  const bool old_allow_exact_keyword_match = input_.allow_exact_keyword_match();
  const bool old_omit_asynchronous_matches = input_.omit_asynchronous_matches();
  const metrics::OmniboxFocusType old_focus_type = input_.focus_type();
  input_ = input;

  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  When the user presses or releases the ctrl key, the desired_tld
  // changes, and when the user finishes an IME composition, inline autocomplete
  // may no longer be prevented.  In both these cases the text itself hasn't
  // changed since the last query, and some providers can do much less work (and
  // get matches back more quickly).  Taking advantage of this reduces flicker.
  //
  // NOTE: This comes after constructing |input_| above since that construction
  // can change the text string (e.g. by stripping off a leading '?').
  const bool minimal_changes =
      (input_.text() == old_input_text) &&
      (input_.allow_exact_keyword_match() == old_allow_exact_keyword_match) &&
      (input_.omit_asynchronous_matches() == old_omit_asynchronous_matches) &&
      (input_.focus_type() == old_focus_type);

  expire_timer_.Stop();
  stop_timer_.Stop();

  // Start the new query.
  sync_pass_done_ = false;
  // Use `start_time` rather than `metrics.start_time_` for
  // 'Omnibox.QueryTime2.*'. They differ by 3 μs, which though too small to be
  // distinguished in the ms-scale buckets, is large enough to move the
  // arithmetic mean.
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Keep a max-heap of negative relevances to quickly estimate a relevance
  // cutoff that can be used to improve counterfactual triggering.
  // Prevent memory churn by starting with full size heap, ready for
  // first change to be pushed without reallocation.
  std::vector<int> relevances(internal_result_.GetDynamicMaxMatches() + 1, 0);
  relevances.pop_back();

  for (const auto& provider : providers_) {
    // Starter Pack engines in keyword mode only run a subset of the providers,
    // so call `ShouldRunProvider()` to determine which ones should run.
    if (!ShouldRunProvider(provider.get())) {
      continue;
    }

    base::TimeTicks provider_start_time = base::TimeTicks::Now();
    if (history_fuzzy_provider_) {
      history_fuzzy_provider_->SetCounterfactualRelevanceHint(
          -relevances.front());
    }
    provider->Start(input_, minimal_changes);

    for (const AutocompleteMatch& match : provider->matches()) {
      relevances.push_back(-match.relevance);
      std::push_heap(relevances.begin(), relevances.end());
      std::pop_heap(relevances.begin(), relevances.end());
      relevances.pop_back();
      DCHECK(std::is_heap(relevances.begin(), relevances.end()));
    }

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

  // This will usually set |done_| to false, unless all providers are finished
  // after the synchronous pass we just completed.
  CheckIfDone();

  // The second true forces saying the default match has changed.
  // This triggers the edit model to update things such as the inline
  // autocomplete state.  In particular, if the user has typed a key
  // since the last notification, and we're now re-running
  // autocomplete, then we need to update the inline autocompletion
  // even if the current match is for the same URL as the last run's
  // default match.  Likewise, the controller doesn't know what's
  // happened in the edit since the last time it ran autocomplete.
  // The user might have selected all the text and hit delete, then
  // typed a new character.  The selection and delete won't send any
  // signals to the controller so it doesn't realize that anything was
  // cleared or changed.  Even if the default match hasn't changed, we
  // need the edit model to update the display.
  UpdateResult(false, true);

  sync_pass_done_ = true;

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

  if (!done_) {
    StartExpireTimer();
    StartStopTimer();
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

void AutocompleteController::Stop(bool clear_result) {
  StopHelper(clear_result, false);
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

  OnProviderUpdate(true, nullptr);

  // If we're not done, we might attempt to redisplay the deleted match. Make
  // sure we aren't displaying it by removing any old entries.
  ExpireCopiedEntries();
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

void AutocompleteController::ExpireCopiedEntries() {
  // The first true makes UpdateResult() clear out the results and
  // regenerate them, thus ensuring that no results from the previous
  // result set remain.
  UpdateResult(true, false);
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
  // perform these operations anyways after all providers are started.
  //
  // This is not a DCHECK, because in the unusual case that a provider calls an
  // asynchronous method, and that method early exits by calling the callback
  // immediately, it's not necessarily a programmer error. We should just no-op.
  if (!sync_pass_done_) {
    return;
  }

  CheckIfDone();

  if (updated_matches || done_)
    UpdateResult(false, false);
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
    UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
        base::TimeDelta query_formulation_time,
        AutocompleteMatch* match) const {
  TRACE_EVENT0("omnibox",
               "AutocompleteController::"
               "UpdateMatchDestinationURLWithAdditionalAssistedQueryStats");
  // The assisted_query_stats is expected to have been previously set when this
  // method is called. If that is not the case, this method is being called by
  // mistake and assisted_query_stats (and searchbox_stats) should not be
  // updated with additional information.
  if (!match->search_terms_args ||
      match->search_terms_args->assisted_query_stats.empty()) {
    return;
  }

  if (match->search_terms_args->searchbox_stats.ByteSizeLong() == 0) {
    NOTREACHED() << "searchbox_stats must be set when assisted_query_stats is.";
    return;
  }

  // Append the query formulation time (time from when the user first typed a
  // character into the omnibox to when the user selected a query), whether
  // a field trial has triggered, and the current page classification to the AQS
  // parameter.
  bool search_feature_triggered =
      triggered_feature_service_->GetFeatureTriggeredInSession(
          metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE) ||
      triggered_feature_service_->GetFeatureTriggeredInSession(
          metrics::OmniboxEventProto_Feature_REMOTE_ZERO_SUGGEST_FEATURE);
  const std::string experiment_stats = base::StringPrintf(
      "%" PRId64 "j%dj%d", query_formulation_time.InMilliseconds(),
      search_feature_triggered, input_.current_page_classification());
  match->search_terms_args->assisted_query_stats += "." + experiment_stats;
  // TODO(crbug.com/1247846): experiment_stats is a deprecated field. We should
  // however continue to report it for parity with what gets reported in aqs=,
  // and for the downstream consumers that expect this field. Once gs_lcrp=
  // fully replaces aqs=, Chrome should start logging the substitute fields and
  // the downstream consumers should migrate to using those fields before we
  // can stop logging this deprecated field.
  match->search_terms_args->searchbox_stats.set_experiment_stats(
      experiment_stats);

  // Append the ExperimentStatsV2 to the AQS parameter to be logged in
  // searchbox_stats.proto's experiment_stats_v2 field.
  if (zero_suggest_provider_) {
    std::vector<std::string> experiment_stats_v2_strings;
    for (const auto& experiment_stat_v2 :
         zero_suggest_provider_->experiment_stats_v2s()) {
      // The string value consists of suggestion type/subtype pairs delimited
      // with colons. However, the SearchboxStats logging flow expects
      // suggestion type/subtype pairs to be delimited with commas instead.
      std::string value = experiment_stat_v2.string_value();
      std::replace(value.begin(), value.end(), ':', ',');
      // The SearchboxStats logging flow expects experiment stats type and value
      // to be delimited with 'i'.
      experiment_stats_v2_strings.push_back(
          base::NumberToString(experiment_stat_v2.type_int()) + "i" + value);
      auto* reported_experiment_stats_v2 =
          match->search_terms_args->searchbox_stats.add_experiment_stats_v2();
      reported_experiment_stats_v2->set_type_int(experiment_stat_v2.type_int());
      reported_experiment_stats_v2->set_string_value(value);
    }
    if (!experiment_stats_v2_strings.empty()) {
      // 'j' is used as a delimiter between individual experiment stat entries.
      match->search_terms_args->assisted_query_stats +=
          "." + base::JoinString(experiment_stats_v2_strings, "j");
    }
  }

  SetMatchDestinationURL(match);
}

void AutocompleteController::SetMatchDestinationURL(
    AutocompleteMatch* match) const {
  TRACE_EVENT0("omnibox", "AutocompleteController::SetMatchDestinationURL");
  auto url = ComputeURLFromSearchTermsArgs(
      match->GetTemplateURL(template_url_service_, false),
      *match->search_terms_args);
  if (url.is_valid()) {
    match->destination_url = std::move(url);
  }
#if BUILDFLAG(IS_ANDROID)
  match->UpdateJavaDestinationUrl();
#endif
}

GURL AutocompleteController::ComputeURLFromSearchTermsArgs(
    TemplateURL* template_url,
    const TemplateURLRef::SearchTermsArgs& search_terms_args) const {
  if (!template_url) {
    return GURL::EmptyGURL();
  }
  return GURL(template_url->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service_->search_terms_data()));
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
    providers_.push_back(new CalculatorProvider(this, search_provider_));
  }
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
  if (provider_types & AutocompleteProvider::TYPE_QUERY_TILE)
    providers_.push_back(new QueryTileProvider(provider_client_.get(), this));
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
}

void AutocompleteController::UpdateResult(
    bool regenerate_result,
    bool force_notify_default_match_changed) {
  // Cancel the scoring model when updating `internal_result_`.
  CancelUrlScoringModel();

  TRACE_EVENT0("omnibox", "AutocompleteController::UpdateResult");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Omnibox.AutocompletionTime.UpdateResult");

  absl::optional<AutocompleteMatch> last_default_match;
  std::u16string last_default_associated_keyword;
  if (internal_result_.default_match()) {
    last_default_match = *internal_result_.default_match();
    if (last_default_match->associated_keyword) {
      last_default_associated_keyword =
          last_default_match->associated_keyword->keyword;
    }
  }

  if (regenerate_result)
    internal_result_.Reset();

  AutocompleteResult old_matches_to_reuse;
  old_matches_to_reuse.Swap(&internal_result_);

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
    }

    internal_result_.MergeSuggestionGroupsMap(
        provider->suggestion_groups_map());
  }

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

  // Conditionally preserve the last default match.
  absl::optional<AutocompleteMatch> default_match_to_preserve;
  if (last_default_match &&
      ShouldPreserveLastDefaultMatch(sync_pass_done_, input_)) {
    default_match_to_preserve = last_default_match;
  }

  // Autocomplete passes can be sync or async.
  // There can be 1 or multiple passes per input.
  // The typical flow is:
  //   1) A sync pass.
  //   2) 1 or more intermediate async passes.
  //   3) 1 last async pass.
  // Another common flow is:
  //   4) A single sync pass.
  // There're other flows, e.g. when expiring transferring matches, but these 2
  // are most common.

  // (1) should:
  //   - deduplicate/sort/cull
  //   - transfer
  //   - deduplicate/sort/cull
  //   - annotate
  //   - notify immediately
  // (2) should:
  //   - deduplicate/sort/cull
  //   - transfer
  //   - deduplicate/sort/cull
  //   - annotate
  //   - notify debounced
  // (3) should:
  //   - deduplicate/sort/cull
  //   - annotate
  //   - notify immediately
  // (3) with ML scoring enabled should:
  //   - deduplicate
  //   - [if `ml_url_scoring_rerank_final_matches_only` is true] sort/cull
  //   - deduplicate/sort/cull
  //   - annotate
  //   - notify immediately
  // (4) should:
  //   - deduplicate/sort/cull
  //   - annotate
  //   - notify immediately
  // There are more steps,e.g. culling tail suggestions, preserving default,
  // demoting entities, grouping, etc, but this is an overview.

  // TODO(manukh): Rewrite this code so the flow is obvious and this comment
  //  becomes unnecessary.

  if (!done_) {
    // Conditionally skip the first call to `SortAndCull()` before the old and
    // new matches are merged.
    static bool single_sort_and_cull_pass =
        base::FeatureList::IsEnabled(omnibox::kSingleSortAndCullPass);
    if (!single_sort_and_cull_pass) {
      internal_result_.SortAndCull(input_, template_url_service_,
                                   triggered_feature_service_,
                                   default_match_to_preserve);
    }
    // If not all providers are done, merge the old and new matches before
    // sorting.
    internal_result_.TransferOldMatches(input_, &old_matches_to_reuse);
  }

  // When sync ML scoring is enabled, run ML scoring in the sync pass and other
  // async update passes. Otherwise, only run ML scoring after all async passes.
  if ((OmniboxFieldTrial::IsMlSyncBatchUrlScoringEnabled() ||
       (done_ && sync_pass_done_ &&
        OmniboxFieldTrial::IsMlUrlScoringEnabled())) &&
      provider_client_->GetAutocompleteScoringModelService()) {
    default_match_to_preserve =
        PreprocessResultForMlScoring(default_match_to_preserve);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    // Use a WeakPtr since the model is not owned and `this` may no longer be
    // alive. `SortCullAndAnnotateResult()` is called when the model is done.
    RunBatchUrlScoringModel(
        base::BindOnce(&AutocompleteController::SortCullAndAnnotateResult,
                       weak_ptr_factory_.GetWeakPtr(), last_default_match,
                       last_default_associated_keyword,
                       force_notify_default_match_changed,
                       default_match_to_preserve),
        OmniboxFieldTrial::IsMlSyncBatchUrlScoringEnabled());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  } else {
    // The final call to `SortAndCull()` happens inside
    // `SortCullAndAnnotateResult()`. Here, the result is sorted, trimmed to a
    // small number of "best" matches, and annotated with relevant information
    // before notifying listeners that the result is ready.
    SortCullAndAnnotateResult(
        last_default_match, last_default_associated_keyword,
        force_notify_default_match_changed, default_match_to_preserve);
  }
}

absl::optional<AutocompleteMatch>
AutocompleteController::PreprocessResultForMlScoring(
    absl::optional<AutocompleteMatch> default_match_to_preserve) {
  const auto& ml_config = OmniboxFieldTrial::GetMLConfig();
  if (ml_config.ml_url_scoring_rerank_final_matches_only) {
    internal_result_.SortAndCull(input_, template_url_service_,
                                 triggered_feature_service_,
                                 default_match_to_preserve);
    if (internal_result_.default_match() &&
        ml_config.ml_url_scoring_preserve_default) {
      default_match_to_preserve = *internal_result_.default_match();
    }
  } else {
    // Deduplicate matches according to `stripped_destination_url` prior to
    // running ML scoring. This step is not needed if `SortAndCull()` is
    // called before the model is executed.
    internal_result_.DeduplicateMatches(input_, template_url_service_);
  }
  return default_match_to_preserve;
}

void AutocompleteController::SortCullAndAnnotateResult(
    const absl::optional<AutocompleteMatch>& last_default_match,
    const std::u16string& last_default_associated_keyword,
    bool force_notify_default_match_changed,
    absl::optional<AutocompleteMatch> default_match_to_preserve) {
  internal_result_.SortAndCull(input_, template_url_service_,
                               triggered_feature_service_,
                               default_match_to_preserve);

#if DCHECK_IS_ON()
  internal_result_.Validate();
#endif  // DCHECK_IS_ON()

  AttachActions();

  UpdateKeywordDescriptions(&internal_result_);
  UpdateAssociatedKeywords(&internal_result_);
  UpdateAssistedQueryStats(&internal_result_);
  UpdateTailSuggestPrefix(&internal_result_);

  if (search_provider_)
    search_provider_->RegisterDisplayedAnswers(internal_result_);

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

  DelayedNotifyChanged(force_notify_default_match_changed ||
                       notify_default_match);
}

void AutocompleteController::AttachActions() {
  if (!input_.IsZeroSuggest()) {
    // Do not look for matching tabs on Android unless we collected all the
    // suggestions. Tab matching is an expensive process with multiple JNI calls
    // involved. Run it only when all the suggestions are collected.
    bool perform_tab_match = is_android ? done_ : true;
    if (perform_tab_match) {
      internal_result_.ConvertOpenTabMatches(provider_client_.get(), &input_);
    }

    internal_result_.AttachPedalsToMatches(input_, *provider_client_);

#if !BUILDFLAG(IS_IOS)
    // HistoryClusters is not enabled on iOS.
    AttachHistoryClustersActions(provider_client_->GetHistoryClustersService(),
                                 internal_result_);
#endif
  }
  internal_result_.TrimOmniboxActions(input_.IsZeroSuggest());

  // TODO(crbug.com/1477861): Eliminate the NTP realbox check when realbox UI
  //  is fixed for this feature. For now the *IncludeRealbox feature param is
  //  defaulted to true so that developers see realbox issues but they can
  //  be prevented in experiments.
  if (OmniboxFieldTrial::IsActionsUISimplificationEnabled() &&
      (OmniboxFieldTrial::kActionsUISimplificationIncludeRealbox.Get() ||
       input_.current_page_classification() !=
           metrics::OmniboxEventProto::NTP_REALBOX)) {
    internal_result_.SplitActionsToSuggestions();
  }
}

void AutocompleteController::UpdateAssociatedKeywords(
    AutocompleteResult* result) {
  if (!keyword_provider_)
    return;

  // Determine if the user's input is an exact keyword match.
  std::u16string exact_keyword =
      keyword_provider_->GetKeywordForText(input_.text());

  std::set<std::u16string> keywords;
  for (auto& match : *result) {
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
      keywords.insert(exact_keyword);
      // If the match has an answer, it will look strange to try to display
      // it along with a keyword hint. Prefer the keyword hint, and revert
      // to a typical search.
      match.answer.reset();
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

    // Only add the keyword if the match does not have a duplicate keyword with
    // a more relevant match.
    if (!keyword.empty() && !keywords.count(keyword)) {
      keywords.insert(keyword);
      match.associated_keyword = std::make_unique<AutocompleteMatch>(
          keyword_provider_->CreateVerbatimMatch(match.fill_into_edit, keyword,
                                                 input_));
    } else {
      match.associated_keyword.reset();
    }
  }
}

void AutocompleteController::UpdateKeywordDescriptions(
    AutocompleteResult* result) {
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

void AutocompleteController::UpdateAssistedQueryStats(
    AutocompleteResult* result) {
  using omnibox::metrics::ChromeSearchboxStats;

  if (result->empty())
    return;

  ChromeSearchboxStats searchbox_stats;
  searchbox_stats.set_client_name("chrome");

  // Build the impressions string (the AQS part after ".").
  int count = 0;
  int num_zero_prefix_suggestions_shown = 0;
  absl::optional<omnibox::SuggestType> last_type;
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

    // Count any suggestions that constitute zero-prefix suggestions.
    if (subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY) ||
        subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS) ||
        subtypes.contains(
            omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES) ||
        subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX)) {
      num_zero_prefix_suggestions_shown++;
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

  // TODO(crbug.com/1307142): These two fields should take into account all the
  // zero-prefix suggestions shown during the session and not only the ones
  // shown at the time of user making a selection.
  searchbox_stats.set_num_zero_prefix_suggestions_shown(
      num_zero_prefix_suggestions_shown);
  searchbox_stats.set_zero_prefix_enabled(num_zero_prefix_suggestions_shown >
                                          0);

  // Go over all matches and set AQS if the match supports it.
  for (size_t index = 0; index < result->size(); ++index) {
    AutocompleteMatch* match = result->match_at(index);
    const TemplateURL* template_url =
        match->GetTemplateURL(template_url_service_, false);
    if (!template_url || !match->search_terms_args)
      continue;

    match->search_terms_args->searchbox_stats = searchbox_stats;

    std::string selected_position;
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

      selected_position = base::StringPrintf("%" PRIuS, match_position);

      // Reconstruct AQS for items sharing the slot (e.g. elements in the
      // carousel).
      if (match_index_belongs_to_horizontal_render_group[index]) {
        aqs[match_index_to_aqs_slot[index]] = ConstructAvailableAutocompletion(
            match->suggest_type, match->subtypes, 1);
      }
    }
    match->search_terms_args->assisted_query_stats =
        base::StringPrintf("chrome.%s.%s", selected_position.c_str(),
                           base::JoinString(aqs, "j").c_str());

    // Duplicate AQS/SBS for eligible ActionsInSuggest.
    // TODO(1418077): rather than computing the `action_uri`, keep the
    // updated search_terms_args, and apply the query formulation time the
    // moment the action is selected.
    for (auto& scoped_action : match->actions) {
      auto* action_in_suggest =
          OmniboxActionInSuggest::FromAction(scoped_action.get());

      if (action_in_suggest == nullptr ||
          !action_in_suggest->search_terms_args.has_value()) {
        continue;
      }
      auto& search_terms_args = action_in_suggest->search_terms_args.value();
      search_terms_args.searchbox_stats.mutable_assisted_query_info()
          ->MergeFrom(
              match->search_terms_args->searchbox_stats.assisted_query_info());
      search_terms_args.assisted_query_stats =
          match->search_terms_args->assisted_query_stats;

      action_in_suggest->action_info.set_action_uri(
          ComputeURLFromSearchTermsArgs(
              match->GetTemplateURL(template_url_service_, false),
              search_terms_args)
              .spec());
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

  published_result_.Swap(&internal_result_);
  internal_result_.CopyFrom(published_result_);

  last_result_for_logging_ = internal_result_.GetMatchDedupComparators();

  for (Observer& obs : observers_)
    obs.OnResultChanged(this, notify_changed_default_match_);
  CancelDelayedNotifyChanged();
}

void AutocompleteController::DelayedNotifyChanged(bool notify_default_match) {
  if (notify_default_match)
    notify_changed_default_match_ = true;
  if (done_ || !sync_pass_done_) {
    notify_changed_debouncer_.ResetTimeLastRun();
    NotifyChanged();
  } else {
    notify_changed_debouncer_.RequestRun(base::BindOnce(
        &AutocompleteController::NotifyChanged, base::Unretained(this)));
  }
}

void AutocompleteController::CancelDelayedNotifyChanged() {
  notify_changed_debouncer_.CancelRequest();
  notify_changed_default_match_ = false;
}

void AutocompleteController::CheckIfDone() {
  bool all_providers_done = true;
  for (const auto& provider : providers_) {
    if (!ShouldRunProvider(provider.get()))
      continue;

    if (!provider->done()) {
      all_providers_done = false;
      break;
    }
  }
  // If asynchronous matches have been disallowed, all providers should be done.
  DCHECK(!input_.omit_asynchronous_matches() || all_providers_done);
  done_ = all_providers_done;
}

void AutocompleteController::StartExpireTimer() {
  // Amount of time (in ms) between when the user stops typing and
  // when we remove any copied entries. We do this from the time the
  // user stopped typing as some providers (such as SearchProvider)
  // wait for the user to stop typing before they initiate a query.
  const int kExpireTimeMS = 500;

  if (internal_result_.HasCopiedMatches())
    expire_timer_.Start(FROM_HERE, base::Milliseconds(kExpireTimeMS), this,
                        &AutocompleteController::ExpireCopiedEntries);
}

void AutocompleteController::StartStopTimer() {
  stop_timer_.Start(FROM_HERE, stop_timer_duration_,
                    base::BindOnce(&AutocompleteController::StopHelper,
                                   base::Unretained(this), false, true));
}

void AutocompleteController::StopHelper(bool clear_result,
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

  expire_timer_.Stop();
  stop_timer_.Stop();
  done_ = true;

  // Cancel any pending requests that may update the results. Otherwise, e.g.,
  // the user's suggestion selection may be reset.
  CancelDelayedNotifyChanged();
  CancelUrlScoringModel();

  if (clear_result && !internal_result_.empty()) {
    internal_result_.Reset();

    // Pass false to clear only the popup and not the edit. Passing true would,
    // e.g., discard the selected suggestion when closing the omnibox.
    DelayedNotifyChanged(false);
  }
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
  NotifyChanged();
  return index;
}

void AutocompleteController::SetSteadyStateOmniboxPosition(
    metrics::OmniboxEventProto::OmniboxPosition position) {
  steady_state_omnibox_position_ = position;
}

bool AutocompleteController::ShouldRunProvider(
    AutocompleteProvider* provider) const {
  if (provider->InKeywordMode(input_)) {
    // Only a subset of providers are run when we're in a starter pack keyword
    // mode. Try to grab the TemplateURL to determine if we're in starter pack
    // mode and whether this provider should be run.
    AutocompleteInput keyword_input = input_;
    const TemplateURL* keyword_turl =
        KeywordProvider::GetSubstitutingTemplateURLForInput(
            template_url_service_, &keyword_input);
    if (keyword_turl && keyword_turl->starter_pack_id() > 0) {
      switch (provider->type()) {
        // Search provider and keyword provider are still run because we would
        // lose the suggestion the keyword chip is attached to otherwise. Search
        // provider suggestions are curbed for starter pack scopes in
        // `SearchProvider::ShouldCurbDefaultSuggestions()`.
        case AutocompleteProvider::TYPE_SEARCH:
        case AutocompleteProvider::TYPE_KEYWORD:
          return true;

        // @Bookmarks starter pack scope - run only the bookmarks provider.
        case AutocompleteProvider::TYPE_BOOKMARK:
          return (keyword_turl->starter_pack_id() ==
                  TemplateURLStarterPackData::kBookmarks);

        // @History starter pack scope - run history quick and history url
        // providers.
        case AutocompleteProvider::TYPE_HISTORY_QUICK:
        case AutocompleteProvider::TYPE_HISTORY_URL:
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
  }

  // Open Tab Provider should only be run for @tabs starter pack mode and in the
  // CrOS launcher.  If we reach here, we're not in starter pack mode, so
  // disable the Open Tab Provider unless we're in the CrOS launcher.
  if (provider->type() == AutocompleteProvider::TYPE_OPEN_TAB &&
      !is_cros_launcher_) {
    return false;
  }

  // Otherwise, run all providers.
  return true;
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void AutocompleteController::RunUrlScoringModel(
    base::OnceClosure completion_callback) {
  TRACE_EVENT0("omnibox", "AutocompleteController::RunUrlScoringModel");

  size_t eligible_matches_count = base::ranges::count_if(
      internal_result_.matches_,
      [](const auto& match) { return match.scoring_signals.has_value(); });

  // If `eligible_matches_count` is 0, `completion_callback` is called
  // immediately.
  auto barrier_callback =
      base::BarrierCallback<AutocompleteScoringModelService::Result>(
          eligible_matches_count,
          base::BindOnce(&AutocompleteController::OnUrlScoringModelDone,
                         weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(),
                         std::move(completion_callback)));

  // Run the model for the eligible matches.
  for (auto& match : internal_result_) {
    if (!match.scoring_signals.has_value()) {
      continue;
    }
    provider_client_->GetAutocompleteScoringModelService()
        ->ScoreAutocompleteUrlMatch(
            &scoring_model_task_tracker_, *match.scoring_signals,
            match.stripped_destination_url.spec(), barrier_callback);
  }
}

void AutocompleteController::RunBatchUrlScoringModel(
    base::OnceClosure completion_callback,
    bool is_sync) {
  TRACE_EVENT0("omnibox", "AutocompleteController::RunBatchUrlScoringModel");

  size_t eligible_matches_count = base::ranges::count_if(
      internal_result_.matches_,
      [](const auto& match) { return match.IsUrlScoringEligible(); });

  // If `eligible_matches_count` is 0, call `completion_callback` immediately.
  if (eligible_matches_count == 0) {
    std::move(completion_callback).Run();
    return;
  }

  // Run the model for the eligible matches.
  std::vector<const ScoringSignals*> batch_scoring_signals;
  batch_scoring_signals.reserve(eligible_matches_count);
  std::vector<std::string> stripped_destination_urls;
  stripped_destination_urls.reserve(eligible_matches_count);
  for (auto& match : internal_result_) {
    if (!match.IsUrlScoringEligible()) {
      continue;
    }
    batch_scoring_signals.push_back(&match.scoring_signals.value());
    stripped_destination_urls.push_back(match.stripped_destination_url.spec());
  }

  auto timer = base::ElapsedTimer();
  if (is_sync) {
    // Synchronous ML model execution.
    const auto batch_results =
        provider_client_->GetAutocompleteScoringModelService()
            ->BatchScoreAutocompleteUrlMatchesSync(
                std::move(batch_scoring_signals),
                std::move(stripped_destination_urls));
    OnUrlScoringModelDone(std::move(timer), std::move(completion_callback),
                          batch_results);
  } else {
    // Async ML model execution.
    provider_client_->GetAutocompleteScoringModelService()
        ->BatchScoreAutocompleteUrlMatches(
            &scoring_model_task_tracker_, std::move(batch_scoring_signals),
            std::move(stripped_destination_urls),
            base::BindOnce(&AutocompleteController::OnUrlScoringModelDone,
                           weak_ptr_factory_.GetWeakPtr(), std::move(timer),
                           std::move(completion_callback)));
  }
}

void AutocompleteController::OnUrlScoringModelDone(
    const base::ElapsedTimer elapsed_timer,
    base::OnceClosure completion_callback,
    std::vector<AutocompleteScoringModelService::Result> results) {
  TRACE_EVENT0("omnibox", "AutocompleteController::OnUrlScoringModelDone");

  // If the model has no predictions, call `completion_callback` immediately.
  if (results.empty()) {
    std::move(completion_callback).Run();
    return;
  }

  // Group the eligible matches by `stripped_destination_url`.
  // TODO(crbug.com/1446688): `stripped_destination_url` is not necessarily
  //  non-empty or unique. A more reliable way to identify the matches will be
  //  needed before ML scoring can be applied to the search suggestions.
  std::map<std::string, ACMatches::iterator> url_to_match_map;
  for (auto match_itr = internal_result_.begin();
       match_itr != internal_result_.end(); ++match_itr) {
    if (!match_itr->scoring_signals.has_value()) {
      continue;
    }

    // Verify the eligible match or one of its duplicates has an expected type.
    DCHECK(match_itr->MatchOrDuplicateMeets([](const auto& match) {
      return AutocompleteScoringSignalsAnnotator::IsEligibleMatch(match);
    })) << "Ineligible "
        << AutocompleteMatchType::ToString(match_itr->type)
        << " match receiving model scoring.";

    // Verify the eligible match has a `stripped_destination_url`. This is
    // computed for the eligible matches before executing the model. No new
    // matches are added since then as the model is executed after all the
    // providers are done.
    DCHECK(!match_itr->stripped_destination_url.is_empty())
        << "ACMatch::stripped_destination_url expected but not present for "
        << AutocompleteMatchType::ToString(match_itr->type);

    url_to_match_map[match_itr->stripped_destination_url.spec()] = match_itr;
  }

  // The goal is to redistribute the existing relevance scores among the
  // eligible matches according to the model prediction scores. Construct two
  // max heaps for the (legacy) relevance score and the model prediction scores.
  std::priority_queue<int> relevance_heap;
  std::priority_queue<std::pair<float, AutocompleteResult::iterator>>
      prediction_and_match_itr_heap;
  for (auto& [prediction, stripped_destination_url] : results) {
    if (!prediction.has_value()) {
      continue;
    }

    // A match with the given stripped destination url is expected to be found.
    if (!base::Contains(url_to_match_map, stripped_destination_url)) {
      NOTREACHED();
      continue;
    }

    auto match_itr = url_to_match_map.at(stripped_destination_url);

    relevance_heap.emplace(match_itr->relevance);
    prediction_and_match_itr_heap.emplace(prediction.value(), match_itr);
  }

  if (!relevance_heap.empty()) {
    // Record whether the model was executed for at least one eligible match.
    provider_client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
        metrics::OmniboxEventProto_Feature_ML_URL_SCORING);

    // Record how many eligible matches the model was executed for.
    base::UmaHistogramCounts1000("Omnibox.URLScoringModelExecuted.Matches",
                                 relevance_heap.size());

    // Record how long it took to execute the model for all eligible matches.
    base::UmaHistogramTimes("Omnibox.URLScoringModelExecuted.ElapsedTime",
                            elapsed_timer.Elapsed());
  }

  while (!relevance_heap.empty()) {
    // If not in the counterfactual treatment, assign the highest relevance
    // score to the match with the highest respective model prediction score.
    if (!OmniboxFieldTrial::IsMlUrlScoringCounterfactual()) {
      auto match_itr = prediction_and_match_itr_heap.top().second;
      match_itr->RecordAdditionalInfo("ml legacy relevance",
                                      match_itr->relevance);
      match_itr->RecordAdditionalInfo(
          "ml model output", (prediction_and_match_itr_heap.top().first * 100));
      match_itr->relevance = relevance_heap.top();
    }
    relevance_heap.pop();
    prediction_and_match_itr_heap.pop();
  }

  std::move(completion_callback).Run();
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void AutocompleteController::CancelUrlScoringModel() {
  // Try to cancel any pending requests to the scoring model and invalidate the
  // WeakPtr to prevent its callbacks from being called.
  scoring_model_task_tracker_.TryCancelAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
}
