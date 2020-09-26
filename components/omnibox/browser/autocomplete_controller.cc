// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <inttypes.h>

#include <cstddef>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/bookmark_provider.h"
#include "components/omnibox/browser/builtin_provider.h"
#include "components/omnibox/browser/clipboard_provider.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/local_history_zero_suggest_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/omnibox/browser/on_device_head_provider.h"
#include "components/omnibox/browser/query_tile_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/shortcuts_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_IOS)
#include "components/open_from_clipboard/clipboard_recent_content_generic.h"
#endif

namespace {

// Converts the given match to a type (and possibly subtype) based on the AQS
// specification. For more details, see
// http://goto.google.com/binary-clients-logging.
// Note: the |subtypes| parameter passed over to this function may be filled
// with subtypes reported by the suggest server. This call will update this set
// with Chrome-specific subtypes.
// TODO(https://crbug.com/1103056): relocate subtype updates to appropriate
// sites that construct these matches.
void GetMatchTypeAndExtendSubtypes(const AutocompleteMatch& match,
                                   size_t* type,
                                   base::flat_set<int>* subtypes) {
  // This type indicates a native chrome suggestion.
  *type = 69;

  // If provider is TYPE_ZERO_SUGGEST or TYPE_ON_DEVICE_HEAD, set the subtype
  // accordingly. Type will be set in the switch statement below where we'll
  // enter one of SEARCH_SUGGEST or NAVSUGGEST.
  if (match.provider) {
    if (match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST &&
        (match.type == AutocompleteMatchType::SEARCH_SUGGEST ||
         match.type == AutocompleteMatchType::NAVSUGGEST)) {
      if (match.type == AutocompleteMatchType::NAVSUGGEST) {
        subtypes->emplace(/*SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS=*/451);
      }
      // We abuse this subtype and use it to for zero-suggest suggestions that
      // aren't personalized by the server. That is, it indicates either
      // client-side most-likely URL suggestions or server-side suggestions
      // that depend only on the URL as context.
      subtypes->emplace(/*SUBTYPE_URL_BASED=*/66);
    } else if (match.provider->type() ==
               AutocompleteProvider::TYPE_ON_DEVICE_HEAD) {
      // This subtype indicates a match from an on-device head provider.
      subtypes->emplace(/*SUBTYPE_SUGGEST_2G_LITE=*/271);
    } else if (match.provider->type() ==
               AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY) {
      subtypes->emplace(/*SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY=*/450);
    }
  }

  switch (match.type) {
    case AutocompleteMatchType::SEARCH_SUGGEST: {
      // Do not set subtype here; subtype may have been set above.
      *type = 0;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY: {
      *type = 46;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL: {
      *type = 33;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED: {
      *type = 35;
      subtypes->emplace(/*SUBTYPE_PERSONAL=*/39);
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE: {
      *type = 44;
      return;
    }
    case AutocompleteMatchType::NAVSUGGEST: {
      // Do not set subtype here; subtype may have been set above.
      *type = 5;
      return;
    }
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_ECHO_SEARCH=*/57);
      return;
    }
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_ECHO_URL=*/58);
      return;
    }
    case AutocompleteMatchType::SEARCH_HISTORY: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_HISTORY_SEARCH=*/59);
      return;
    }
    case AutocompleteMatchType::HISTORY_URL: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_HISTORY_URL=*/60);
      return;
    }
    case AutocompleteMatchType::HISTORY_TITLE: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_HISTORY_TITLE=*/61);
      return;
    }
    case AutocompleteMatchType::HISTORY_BODY: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_HISTORY_BODY=*/62);
      return;
    }
    case AutocompleteMatchType::HISTORY_KEYWORD: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_HISTORY_KEYWORD=*/63);
      return;
    }
    case AutocompleteMatchType::BOOKMARK_TITLE: {
      subtypes->emplace(/*SUBTYPE_OMNIBOX_BOOKMARK_TITLE=*/65);
      return;
    }
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED: {
      *type = 5;
      subtypes->emplace(/*SUBTYPE_PERSONAL=*/39);
      return;
    }
    case AutocompleteMatchType::CALCULATOR: {
      *type = 6;
      return;
    }
    case AutocompleteMatchType::CLIPBOARD_URL: {
      subtypes->emplace(/*SUBTYPE_CLIPBOARD_URL=*/177);
      return;
    }
    case AutocompleteMatchType::CLIPBOARD_TEXT: {
      subtypes->emplace(/*SUBTYPE_CLIPBOARD_TEXT=*/176);
      return;
    }
    case AutocompleteMatchType::CLIPBOARD_IMAGE: {
      subtypes->emplace(/*SUBTYPE_CLIPBOARD_IMAGE=*/327);
      return;
    }
    case AutocompleteMatchType::TILE_SUGGESTION: {
      *type = 171;
      return;
    }
    default: {
      // This value indicates a native chrome suggestion with no named subtype
      // (yet).
      subtypes->emplace(/*SUBTYPE_OMNIBOX_OTHER=*/64);
    }
  }
}

// Appends available autocompletion of the given type, subtype, and number to
// the existing available autocompletions string, encoding according to the
// spec.
void AppendAvailableAutocompletion(size_t type,
                                   const base::flat_set<int>& subtypes,
                                   int count,
                                   std::string* autocompletions) {
  if (!autocompletions->empty())
    autocompletions->append("j");
  base::StringAppendF(autocompletions, "%" PRIuS, type);

  std::ostringstream subtype_str;
  for (auto subtype : subtypes) {
    if (subtype_str.tellp() > 0)
      subtype_str << 'i';
    subtype_str << subtype;
  }

  // Subtype is optional. Append only if we have subtypes to report.
  if (subtype_str.tellp() > 0)
    base::StringAppendF(autocompletions, "i%s", subtype_str.str().c_str());

  if (count > 1)
    base::StringAppendF(autocompletions, "l%d", count);
}

// Whether this autocomplete match type supports custom descriptions.
bool AutocompleteMatchHasCustomDescription(const AutocompleteMatch& match) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP &&
      match.type == AutocompleteMatchType::CALCULATOR) {
    return true;
  }
  return match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         match.type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
}

}  // namespace

AutocompleteController::AutocompleteController(
    std::unique_ptr<AutocompleteProviderClient> provider_client,
    int provider_types)
    : provider_client_(std::move(provider_client)),
      document_provider_(nullptr),
      history_url_provider_(nullptr),
      keyword_provider_(nullptr),
      search_provider_(nullptr),
      zero_suggest_provider_(nullptr),
      on_device_head_provider_(nullptr),
      stop_timer_duration_(OmniboxFieldTrial::StopTimerFieldTrialDuration()),
      done_(true),
      in_start_(false),
      first_query_(true),
      search_service_worker_signal_sent_(false),
      template_url_service_(provider_client_->GetTemplateURLService()) {
  provider_types &= ~OmniboxFieldTrial::GetDisabledProviderTypes();
  if (provider_types & AutocompleteProvider::TYPE_BOOKMARK)
    providers_.push_back(new BookmarkProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_BUILTIN)
    providers_.push_back(new BuiltinProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_QUICK)
    providers_.push_back(new HistoryQuickProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_KEYWORD) {
    keyword_provider_ = new KeywordProvider(provider_client_.get(), this);
    providers_.push_back(keyword_provider_);
  }
  if (provider_types & AutocompleteProvider::TYPE_SEARCH) {
    search_provider_ = new SearchProvider(provider_client_.get(), this);
    providers_.push_back(search_provider_);
  }
  // It's important that the HistoryURLProvider gets added after SearchProvider:
  // AutocompleteController::Start() calls each providers' Start() function
  // synchronously in the order they're in in providers_.
  // - SearchProvider::Start() synchronously queries the history database's
  //   keyword_search_terms and url table.
  // - HistoryUrlProvider::Start schedules a background task that also accesses
  //   the history database.
  // If both db accesses happen concurrently, TSan complains.
  // So put HistoryURLProvider later to make sure that SearchProvider is done
  // doing its thing by the time the HistoryURLProvider task runs.
  // (And hope that it completes before AutocompleteController::Start() is
  // called the next time.)
  // ClipboardURLProvider take a reference to HistoryURLProvider. If we're going
  // to need it, we should initialize history_url_provider_.
  if (provider_types & (AutocompleteProvider::TYPE_HISTORY_URL |
                        AutocompleteProvider::TYPE_CLIPBOARD)) {
    history_url_provider_ =
        new HistoryURLProvider(provider_client_.get(), this);
    if (provider_types & AutocompleteProvider::TYPE_HISTORY_URL)
      providers_.push_back(history_url_provider_);
  }
  if (provider_types & AutocompleteProvider::TYPE_SHORTCUTS)
    providers_.push_back(new ShortcutsProvider(provider_client_.get()));
  if (provider_types & AutocompleteProvider::TYPE_ZERO_SUGGEST) {
    zero_suggest_provider_ =
        ZeroSuggestProvider::Create(provider_client_.get(), this);
    if (zero_suggest_provider_)
      providers_.push_back(zero_suggest_provider_);
#if defined(OS_ANDROID)
    // Note: the need for the always-present verbatim match originates from the
    // OmniboxSearchReadyIncognito feature.
    // The feature aims at showing SRO in an Incognito mode, where the
    // ZeroSuggestProvider intentionally never gets invoked.
    // The gating flag here should be removed when the SRO Incognito is
    // launched.
    if (base::FeatureList::IsEnabled(omnibox::kOmniboxSearchReadyIncognito)) {
      providers_.push_back(
          new ZeroSuggestVerbatimMatchProvider(provider_client_.get()));
    }
#endif
  }
  if (provider_types & AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY) {
    providers_.push_back(
        LocalHistoryZeroSuggestProvider::Create(provider_client_.get(), this));
  }
  if (provider_types & AutocompleteProvider::TYPE_DOCUMENT) {
    document_provider_ = DocumentProvider::Create(provider_client_.get(), this);
    providers_.push_back(document_provider_);
  }
  if (provider_types & AutocompleteProvider::TYPE_ON_DEVICE_HEAD) {
    on_device_head_provider_ =
        OnDeviceHeadProvider::Create(provider_client_.get(), this);
    if (on_device_head_provider_) {
      providers_.push_back(on_device_head_provider_);
      on_device_head_provider_->AddModelUpdateCallback();
    }
  }
  if (provider_types & AutocompleteProvider::TYPE_CLIPBOARD) {
#if !defined(OS_IOS)
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
          provider_client_.get(), this, history_url_provider_,
          ClipboardRecentContent::GetInstance());
      providers_.push_back(clipboard_provider_);
    }
  }

  if (provider_types & AutocompleteProvider::TYPE_QUERY_TILE)
    providers_.push_back(new QueryTileProvider(provider_client_.get(), this));

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "AutocompleteController", base::ThreadTaskRunnerHandle::Get());
}

AutocompleteController::~AutocompleteController() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  // The providers may have tasks outstanding that hold refs to them.  We need
  // to ensure they won't call us back if they outlive us.  (Practically,
  // calling Stop() should also cancel those tasks and make it so that we hold
  // the only refs.)  We also don't want to bother notifying anyone of our
  // result changes here, because the notification observer is in the midst of
  // shutdown too, so we don't ask Stop() to clear |result_| (and notify).
  result_.Reset();  // Not really necessary.
  Stop(false);
}

void AutocompleteController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AutocompleteController::Start(const AutocompleteInput& input) {
  TRACE_EVENT1("omnibox", "AutocompleteController::Start",
               "text", base::UTF16ToUTF8(input.text()));

  // When input.want_asynchronous_matches() is false, the AutocompleteController
  // is being used for text classification, which should not notify observers.
  if (input.want_asynchronous_matches()) {
    for (Observer& obs : observers_)
      obs.OnStart(this, input);
  }

  const base::string16 old_input_text(input_.text());
  const bool old_allow_exact_keyword_match = input_.allow_exact_keyword_match();
  const bool old_want_asynchronous_matches = input_.want_asynchronous_matches();
  const OmniboxFocusType old_focus_type = input_.focus_type();
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
      (input_.want_asynchronous_matches() == old_want_asynchronous_matches) &&
      (input_.focus_type() == old_focus_type);

  expire_timer_.Stop();
  stop_timer_.Stop();

  // Start the new query.
  in_start_ = true;
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (auto i(providers_.begin()); i != providers_.end(); ++i) {
    base::TimeTicks provider_start_time = base::TimeTicks::Now();
    (*i)->Start(input_, minimal_changes);
    if (!input.want_asynchronous_matches())
      DCHECK((*i)->done());
    base::TimeTicks provider_end_time = base::TimeTicks::Now();
    std::string name = std::string("Omnibox.ProviderTime2.") + (*i)->GetName();
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 5000, 20, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>(
        (provider_end_time - provider_start_time).InMilliseconds()));
  }
  if (input.want_asynchronous_matches() && (input.text().length() < 6)) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name =
        "Omnibox.QueryTime2." + base::NumberToString(input.text().length());
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
  base::UmaHistogramBoolean("Omnibox.Start.WantAsyncMatches",
                            input.want_asynchronous_matches());

  // This will usually set |done_| to false, unless all of the providers are
  // are finished after the synchronous pass we just completed.
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

  in_start_ = false;

  // Omnibox has dependencies that may be lazily initialized. This metric will
  // help tracking regression on the first use.
  if (first_query_) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    base::HistogramBase* counter =
        base::Histogram::FactoryGet("Omnibox.WarmupTime", 1, 1000, 50,
                                    base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
    first_query_ = false;
  }

  // If the input looks like a query, send a signal predicting that the user is
  // going to issue a search (either to the default search engine or to a
  // keyword search engine, as indicated by the destination_url). This allows
  // any associated service worker to start up early and reduce the latency of a
  // resulting search. However, to avoid a potentially expensive operation, we
  // only do this once per session. Additionally, a default match is expected to
  // be available at this point but we check anyway to guard against an invalid
  // dereference.
  if (base::FeatureList::IsEnabled(
          omnibox::kSpeculativeServiceWorkerStartOnQueryInput) &&
      input.type() == metrics::OmniboxInputType::QUERY &&
      !search_service_worker_signal_sent_ && result_.default_match()) {
    search_service_worker_signal_sent_ = true;
    provider_client_->StartServiceWorker(
        result_.default_match()->destination_url);
  }

  if (!done_) {
    StartExpireTimer();
    StartStopTimer();
  }
}

void AutocompleteController::Stop(bool clear_result) {
  StopHelper(clear_result, false);
}

void AutocompleteController::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.SupportsDeletion());

  // This formula combines provider and result type into a single enum as
  // defined in OmniboxProviderAndResultType in enums.xml.
  auto combined_type = match.provider->AsOmniboxEventProviderType() * 100 +
                       match.AsOmniboxEventResultType();

  // Delete duplicate matches attached to the main match first.
  for (auto it(match.duplicate_matches.begin());
       it != match.duplicate_matches.end(); ++it) {
    if (it->deletable)
      it->provider->DeleteMatch(*it);
  }

  if (match.deletable) {
    // This histogram is defined in the internal histograms.xml. This is because
    // the vast majority of OmniboxProviderAndResultType histograms are
    // generated by internal tools, and we wish to keep them together.
    base::UmaHistogramSparse("Omnibox.SuggestionDeleted.ProviderAndResultType",
                             combined_type);
    match.provider->DeleteMatch(match);
  }

  OnProviderUpdate(true);

  // If we're not done, we might attempt to redisplay the deleted match. Make
  // sure we aren't displaying it by removing any old entries.
  ExpireCopiedEntries();
}

void AutocompleteController::ExpireCopiedEntries() {
  // The first true makes UpdateResult() clear out the results and
  // regenerate them, thus ensuring that no results from the previous
  // result set remain.
  UpdateResult(true, false);
}

void AutocompleteController::OnProviderUpdate(bool updated_matches) {
  // Providers should only call this method during the asynchronous pass.
  // There's no reason to call this during the synchronous pass, since we
  // perform these operations anyways after all providers are started.
  //
  // This is not a DCHECK, because in the unusual case that a provider calls an
  // asynchronous method, and that method early exits by calling the callback
  // immediately, it's not necessarily a programmer error. We should just no-op.
  if (in_start_)
    return;

  CheckIfDone();
  // Multiple providers may provide synchronous results, so we only update the
  // results if we're not in Start().
  if (updated_matches || done_)
    UpdateResult(false, false);
}

void AutocompleteController::AddProvidersInfo(
    ProvidersInfo* provider_info) const {
  provider_info->clear();
  for (auto i(providers_.begin()); i != providers_.end(); ++i) {
    // Add per-provider info, if any.
    (*i)->AddProviderInfo(provider_info);

    // This is also a good place to put code to add info that you want to
    // add for every provider.
  }

  if (OmniboxFieldTrial::IsPedalSuggestionsEnabled()) {
    // OmniboxPedalProvider is not a "true" AutocompleteProvider and isn't
    // included in the list of providers, though needs to report information for
    // its field trial.  Manually call AddProviderInfo for pedals.
    provider_client_->GetPedalProvider()->AddProviderInfo(provider_info);
  }
}

void AutocompleteController::ResetSession() {
  search_service_worker_signal_sent_ = false;

  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->ResetSession();
  }

  if (OmniboxFieldTrial::IsPedalSuggestionsEnabled()) {
    // OmniboxPedalProvider is not included in the list of providers as it's not
    // a "true" AutocompleteProvider.  Manually call ResetSession() for pedals.
    provider_client_->GetPedalProvider()->ResetSession();
  }
}

void AutocompleteController::UpdateMatchDestinationURLWithQueryFormulationTime(
    base::TimeDelta query_formulation_time,
    AutocompleteMatch* match) const {
  if (!match->search_terms_args ||
      match->search_terms_args->assisted_query_stats.empty())
    return;

  // Append the query formulation time (time from when the user first typed a
  // character into the omnibox to when the user selected a query) and whether
  // a field trial has triggered to the AQS parameter.
  TemplateURLRef::SearchTermsArgs search_terms_args(*match->search_terms_args);
  search_terms_args.assisted_query_stats += base::StringPrintf(
      ".%" PRId64 "j%dj%d",
      query_formulation_time.InMilliseconds(),
      (search_provider_ &&
       search_provider_->field_trial_triggered_in_session()) ||
      (zero_suggest_provider_ &&
       zero_suggest_provider_->field_trial_triggered_in_session()),
      input_.current_page_classification());

  // Append the experiment stats to the AQS parameter to be logged in
  // searchbox_stats.proto's experiment_stats_v2 field.
  if (zero_suggest_provider_) {
    // The field number for the experiment stat type specified as an int
    // in ExperimentStatsV2.
    constexpr char kTypeIntFieldNumber[] = "4";
    // The field number for the string value in ExperimentStatsV2.
    constexpr char kStringValueFieldNumber[] = "2";
    std::vector<std::string> experiment_stats_v2;
    for (const auto& experiment_stat :
         zero_suggest_provider_->experiment_stats()) {
      DCHECK(experiment_stat.is_dict());
      base::Optional<int> type_int =
          experiment_stat.FindIntPath(kTypeIntFieldNumber);
      const std::string* string_value =
          experiment_stat.FindStringPath(kStringValueFieldNumber);
      if (type_int && string_value) {
        // The string value consists of suggestion type/subtype pairs which are
        // delimited with colons. Replace colons with commas as expected by the
        // Searchbox logging flow.
        std::string value = *string_value;
        std::replace(value.begin(), value.end(), ':', ',');
        // 'i' is used as a delimiter between experiment stat type and value.
        experiment_stats_v2.push_back(base::NumberToString(*type_int) + "i" +
                                      value);
      }
    }
    if (!experiment_stats_v2.empty()) {
      // 'j' is used as a delimiter between individual experiment stat entries.
      search_terms_args.assisted_query_stats +=
          "." + base::JoinString(experiment_stats_v2, "j");
    }
  }

  UpdateMatchDestinationURL(search_terms_args, match);
}

void AutocompleteController::UpdateMatchDestinationURL(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    AutocompleteMatch* match) const {
  const TemplateURL* template_url = match->GetTemplateURL(
      template_url_service_, false);
  if (!template_url)
    return;

  match->destination_url = GURL(template_url->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service_->search_terms_data()));
}

void AutocompleteController::InlineTailPrefixes() {
  result_.InlineTailPrefixes();
}

void AutocompleteController::UpdateResult(
    bool regenerate_result,
    bool force_notify_default_match_changed) {
  TRACE_EVENT0("omnibox", "AutocompleteController::UpdateResult");

  base::Optional<AutocompleteMatch> last_default_match;
  base::string16 last_default_associated_keyword;
  if (result_.default_match()) {
    last_default_match = *result_.default_match();
    if (last_default_match->associated_keyword) {
      last_default_associated_keyword =
          last_default_match->associated_keyword->keyword;
    }
  }

  const auto last_result_for_logging = result_.GetMatchDedupComparators();

  if (regenerate_result)
    result_.Reset();

  AutocompleteResult old_matches_to_reuse;
  old_matches_to_reuse.Swap(&result_);

  for (Providers::const_iterator i(providers_.begin());
       i != providers_.end(); ++i)
    result_.AppendMatches(input_, (*i)->matches());

  if (OmniboxFieldTrial::IsTabSwitchSuggestionsEnabled())
    result_.ConvertOpenTabMatches(provider_client_.get(), &input_);

  UpdateHeaderInfoFromZeroSuggestProvider(&result_);

  // Sort the matches and trim to a small number of "best" matches.
  const AutocompleteMatch* preserve_default_match = nullptr;
  if (!in_start_ && last_default_match) {
    preserve_default_match = &last_default_match.value();
  }
  result_.SortAndCull(input_, template_url_service_, preserve_default_match);

  if (OmniboxFieldTrial::IsPedalSuggestionsEnabled()) {
    result_.AttachPedalsToMatches(input_, *provider_client_);
  }

  // Need to validate before invoking CopyOldMatches as the old matches are not
  // valid against the current input.
#if DCHECK_IS_ON()
  result_.Validate();
#endif  // DCHECK_IS_ON()

  if (!done_) {
    // This conditional needs to match the conditional in Start that invokes
    // StartExpireTimer.
    result_.TransferOldMatches(input_, &old_matches_to_reuse,
                               template_url_service_);
  }

  // Log metrics for how many matches are asynchronously changed.
  if (!in_start_) {
    AutocompleteResult::LogAsynchronousUpdateMetrics(last_result_for_logging,
                                                     result_);
  }

  UpdateKeywordDescriptions(&result_);
  UpdateAssociatedKeywords(&result_);
  UpdateAssistedQueryStats(&result_);
  if (search_provider_)
    search_provider_->RegisterDisplayedAnswers(result_);

  const bool default_is_valid = result_.default_match();
  base::string16 default_associated_keyword;
  if (default_is_valid &&
      result_.default_match()->associated_keyword) {
    default_associated_keyword =
        result_.default_match()->associated_keyword->keyword;
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
       ((result_.default_match()->fill_into_edit !=
         last_default_match->fill_into_edit) ||
        (default_associated_keyword != last_default_associated_keyword) ||
        (result_.default_match()->keyword != last_default_match->keyword)));
  if (notify_default_match)
    last_time_default_match_changed_ = base::TimeTicks::Now();

  NotifyChanged(force_notify_default_match_changed || notify_default_match);
}

void AutocompleteController::UpdateAssociatedKeywords(
    AutocompleteResult* result) {
  if (!keyword_provider_)
    return;

  // Determine if the user's input is an exact keyword match.
  base::string16 exact_keyword =
      keyword_provider_->GetKeywordForText(input_.text());

  std::set<base::string16> keywords;
  for (auto match(result->begin()); match != result->end(); ++match) {
    base::string16 keyword(
        match->GetSubstitutingExplicitlyInvokedKeyword(template_url_service_));
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
      match->answer.reset();
      match->associated_keyword.reset(new AutocompleteMatch(
          keyword_provider_->CreateVerbatimMatch(exact_keyword,
                                                 exact_keyword, input_)));
      continue;
    }

    // Otherwise, set a match's associated keyword based on the match's
    // fill_into_edit, which should take inline autocompletions into account.
    keyword = keyword_provider_->GetKeywordForText(match->fill_into_edit);

    // Only add the keyword if the match does not have a duplicate keyword with
    // a more relevant match.
    if (!keyword.empty() && !keywords.count(keyword)) {
      keywords.insert(keyword);
      match->associated_keyword.reset(new AutocompleteMatch(
          keyword_provider_->CreateVerbatimMatch(match->fill_into_edit,
                                                 keyword, input_)));
    } else {
      match->associated_keyword.reset();
    }
  }
}

void AutocompleteController::UpdateHeaderInfoFromZeroSuggestProvider(
    AutocompleteResult* result) {
  // Currently, we only populate the AutocompleteResult's header labels from
  // ZeroSuggestProvider. Even if another provider has header metadata, we
  // currently ignore it. This means that as-you-type suggestions will NEVER
  // show headers in the UI. For now, this is hacky, but intended.
  //
  // TODO(tommycli): Stop special casing ZeroSuggestProvider here.
  if (!zero_suggest_provider_)
    return;

  // Merge the new header info with the existing one rather than replacing it.
  // We might end up using the existing matches fully or partially if there are
  // not enough new ones. Thus, we should also keep the existing header info.
  result->MergeHeadersMap(zero_suggest_provider_->headers_map());
  result->MergeHiddenGroupIds(zero_suggest_provider_->hidden_group_ids());
}

void AutocompleteController::UpdateKeywordDescriptions(
    AutocompleteResult* result) {
  base::string16 last_keyword;
  for (auto i(result->begin()); i != result->end(); ++i) {
    if (AutocompleteMatch::IsSearchType(i->type)) {
      if (AutocompleteMatchHasCustomDescription(*i))
        continue;
      i->description.clear();
      i->description_class.clear();
      DCHECK(!i->keyword.empty());
      if (i->keyword != last_keyword &&
          !ShouldCurbKeywordDescriptions(i->keyword)) {
        const TemplateURL* template_url =
            i->GetTemplateURL(template_url_service_, false);
        if (template_url) {
          // For extension keywords, just make the description the extension
          // name -- don't assume that the normal search keyword description is
          // applicable.
          i->description = template_url->AdjustedShortNameForLocaleDirection();
          if (template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION) {
            i->description = l10n_util::GetStringFUTF16(
                IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION, i->description);
          }
          i->description_class.push_back(
              ACMatchClassification(0, ACMatchClassification::DIM));
        }
        last_keyword = i->keyword;
      }
    } else {
      last_keyword.clear();
    }
  }
}

void AutocompleteController::UpdateAssistedQueryStats(
    AutocompleteResult* result) {
  if (result->empty())
    return;

  // Build the impressions string (the AQS part after ".").
  std::string autocompletions;
  int count = 0;
  int num_zero_prefix_shown = 0;
  size_t last_type = base::string16::npos;
  base::flat_set<int> last_subtypes = {};
  for (const auto& match : *result) {
    auto subtypes = match.subtypes;
    size_t type = base::string16::npos;
    GetMatchTypeAndExtendSubtypes(match, &type, &subtypes);

    // Count any suggestions that constitute zero-prefix suggestions.
    if (match.subtypes.contains(/*SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY=*/450) ||
        match.subtypes.contains(
            /*SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS=*/451) ||
        match.subtypes.contains(/*SUBTYPE_ZERO_PREFIX=*/362)) {
      ++num_zero_prefix_shown;
    }

    if (last_type != base::string16::npos &&
        (type != last_type || subtypes != last_subtypes)) {
      AppendAvailableAutocompletion(last_type, last_subtypes, count,
                                    &autocompletions);
      count = 1;
    } else {
      count++;
    }
    last_type = type;
    last_subtypes = subtypes;
  }
  AppendAvailableAutocompletion(last_type, last_subtypes, count,
                                &autocompletions);
  // Go over all matches and set AQS if the match supports it.
  for (size_t index = 0; index < result->size(); ++index) {
    AutocompleteMatch* match = result->match_at(index);
    const TemplateURL* template_url =
        match->GetTemplateURL(template_url_service_, false);
    if (!template_url || !match->search_terms_args)
      continue;
    std::string selected_index;
    // Prevent trivial suggestions from getting credit for being selected.
    if (!match->IsTrivialAutocompletion())
      selected_index = base::StringPrintf("%" PRIuS, index);
    match->search_terms_args->assisted_query_stats =
        base::StringPrintf("chrome.%s.%s",
                           selected_index.c_str(),
                           autocompletions.c_str());

    if (num_zero_prefix_shown > 0) {
      // Note: 1st skipped parameter: EXPERIMENT_STATS.
      // Note: 2nd skipped parameter: SINGLE_SEARCHBOX_CONTENT.
      match->search_terms_args->assisted_query_stats +=
          base::StringPrintf("...%d", num_zero_prefix_shown);
    }

    match->destination_url = GURL(template_url->url_ref().ReplaceSearchTerms(
        *match->search_terms_args, template_url_service_->search_terms_data()));
  }
}

void AutocompleteController::NotifyChanged(bool notify_default_match) {
  for (Observer& obs : observers_)
    obs.OnResultChanged(this, notify_default_match);
  if (done_)
    provider_client_->OnAutocompleteControllerResultReady(this);
}

void AutocompleteController::CheckIfDone() {
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    if (!(*i)->done()) {
      done_ = false;
      return;
    }
  }
  done_ = true;
}

void AutocompleteController::StartExpireTimer() {
  // Amount of time (in ms) between when the user stops typing and
  // when we remove any copied entries. We do this from the time the
  // user stopped typing as some providers (such as SearchProvider)
  // wait for the user to stop typing before they initiate a query.
  const int kExpireTimeMS = 500;

  if (result_.HasCopiedMatches())
    expire_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kExpireTimeMS),
                        this, &AutocompleteController::ExpireCopiedEntries);
}

void AutocompleteController::StartStopTimer() {
  stop_timer_.Start(FROM_HERE, stop_timer_duration_,
                    base::BindOnce(&AutocompleteController::StopHelper,
                                   base::Unretained(this), false, true));
}

void AutocompleteController::StopHelper(bool clear_result,
                                        bool due_to_user_inactivity) {
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Stop(clear_result, due_to_user_inactivity);
  }

  expire_timer_.Stop();
  stop_timer_.Stop();
  done_ = true;
  if (clear_result && !result_.empty()) {
    result_.Reset();
    // NOTE: We pass in false since we're trying to only clear the popup, not
    // touch the edit... this is all a mess and should be cleaned up :(
    NotifyChanged(false);
  }
}

bool AutocompleteController::ShouldCurbKeywordDescriptions(
    const base::string16& keyword) {
  return AutocompleteProvider::InExplicitExperimentalKeywordMode(input_,
                                                                 keyword);
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
  res += result_.EstimateMemoryUsage();

  auto* dump = process_memory_dump->CreateAllocatorDump(
      base::StringPrintf("omnibox/autocomplete_controller/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, res);
  return true;
}
