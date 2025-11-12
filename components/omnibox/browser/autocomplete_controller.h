// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_controller_metrics.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "third_party/omnibox_proto/types.pb.h"

class BookmarkProvider;
class ClipboardProvider;
class ContextualSearchProvider;
class DocumentProvider;
class FeaturedSearchProvider;
class HistoryFuzzyProvider;
class HistoryQuickProvider;
class HistoryURLProvider;
class KeywordProvider;
class OmniboxTriggeredFeatureService;
class OnDeviceHeadProvider;
class OpenTabProvider;
class SearchProvider;
class TabGroupProvider;
class TemplateURLService;
class VoiceSuggestProvider;
class ZeroSuggestProvider;
struct OmniboxLog;

namespace extensions {
class UnscopedOmniboxApiTest;
}  // namespace extensions

// The header used to report whether a navigation to google.com is coming from
// omnibox. Only set when the navigation is initiated from the Gemini
// built-in keyword.
inline constexpr char kOmniboxGeminiHeader[] = "X-Omnibox-Gemini";

// The AutocompleteController is the center of the autocomplete system.  A
// class creates an instance of the controller, which in turn creates a set of
// AutocompleteProviders to serve it.  The owning class can ask the controller
// to Start() a query; the controller in turn passes this call down to the
// providers, each of which keeps track of its own matches and whether it has
// finished processing the query.  When a provider gets more matches or finishes
// processing, it notifies the controller, which merges the combined matches
// together and makes the result available to interested observers.
//
// The owner may also cancel the current query by calling Stop(), which the
// controller will in turn communicate to all the providers.  No callbacks will
// happen after a request has been stopped.
//
// IMPORTANT: There is NO THREAD SAFETY built into this portion of the
// autocomplete system.  All calls to and from the AutocompleteController should
// happen on the same thread.  AutocompleteProviders are responsible for doing
// their own thread management when they need to return matches asynchronously.
//
// The coordinator for autocomplete queries, responsible for combining the
// matches from a series of providers into one AutocompleteResult.
class AutocompleteController : public AutocompleteProviderListener,
                               public base::trace_event::MemoryDumpProvider {
  // TODO(crbug.com/449894891): Remove this macro once it gets fixed.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  // Describes an autocomplete pass.
  enum class UpdateType {
    // The 'null' update; used by `last_update_type_` to indicate no update has
    // occurred for the current input.
    kNone,
    // An update triggered by the initial sync pass completing with all
    // providers being done. I.e. no async pass will follow; e.g. because the
    // user deleted text.
    kSyncPassOnly,
    // An update triggered by the initial sync pass completing without all
    // providers being done. I.e., async passes will follow if not interrupted.
    kSyncPass,
    // An update triggered by an async pass completing without all providers
    // being done.
    kAsyncPass,
    // An update triggered by an async pass completing with all providers except
    // the doc provider being done.
    kLastAsyncPassExceptDoc,
    // An update triggered by the expire timer to clear transferred matches.
    kExpirePass,
    // An update triggered by an async pass completing with all providers being
    // done.
    kLastAsyncPass,
    // An update caused by stopping autocompletion either due to user
    // interaction or user inactivity.
    kStop,
    // An update triggered by the user deleting a match.
    kMatchDeletion,
  };

  using Providers = std::vector<scoped_refptr<AutocompleteProvider>>;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the |controller| Start() is called with an |input| that
    // wants asynchronous matches. This is meant to exclude text classification
    // requests. The |controller| parameter is only useful for observers that
    // are observing multiple AutocompleteController instances.
    virtual void OnStart(AutocompleteController* controller,
                         const AutocompleteInput& input) {}

    // Invoked when the result set of |controller| changes. If
    // |default_match_changed| is true, the default match of the result set has
    // changed. The |controller| parameter is only useful for observers that
    // are observing multiple AutocompleteController instances.
    virtual void OnResultChanged(AutocompleteController* controller,
                                 bool default_match_changed) {}

    // Invoked when a ML scoring batch completes, i.e. `OnUrlScoringModelDone()`
    // completes.
    virtual void OnMlScored(AutocompleteController* controller,
                            const AutocompleteResult& result) {}

    // Invoked when autocomplete stop timer is triggered.
    virtual void OnAutocompleteStopTimerTriggered(
        const AutocompleteInput& input) {}
  };

  // Converts `UpdateType` to string.
  static std::string UpdateTypeToDebugString(UpdateType update_type);

  // Given a match, returns zero or more subtypes corresponding to SuggestType
  // and SuggestSubtype enums in //third_party/omnibox_proto/types.proto.
  // This is needed to update Chrome's native types/subtypes to those expected
  // by the server. For more details, see go/chrome-suggest-logging.
  // Note: `subtypes` may be prepopulated with server-reported subtypes.
  static void ExtendMatchSubtypes(
      const AutocompleteMatch& match,
      base::flat_set<omnibox::SuggestSubtype>* subtypes);

  // Given an `ml_score` in the range [0, 1], computes the corresponding
  // relevance score using the piecewise function described by the given
  // `break_points`.
  static int ApplyPiecewiseScoringTransform(
      double ml_score,
      std::vector<std::pair<double, int>> break_points);

  // `provider_client` is passed to all providers.`config` customizes
  // autocomplete behavior for its embedders.
  AutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      const AutocompleteControllerConfig& config);
  ~AutocompleteController() override;
  AutocompleteController(const AutocompleteController&) = delete;
  AutocompleteController& operator=(const AutocompleteController&) = delete;

  // UI elements that need to be notified when the results get updated should
  // be added as an |observer|.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts an autocomplete query, which continues until all providers are
  // done or the query is Stop()ed.  It is safe to Start() a new query without
  // Stop()ing the previous one.
  //
  // See AutocompleteInput::AutocompleteInput(...) for more details regarding
  // |input| params.
  //
  // The controller calls AutocompleteController::Observer::OnResultChanged()
  // from inside this call at least once. If matches are available later on that
  // result in changing the result set the observers is notified again. When the
  // controller is done the notification AUTOCOMPLETE_CONTROLLER_RESULT_READY is
  // sent.
  // Made virtual for mocking in tests.
  virtual void Start(const AutocompleteInput& input);

  // Calls StartPrefetch() on all eligible providers so that they can optionally
  // perform a prefetch request to warm up their underlying service(s) and/or
  // optionally cache their otherwise async response.
  // Made virtual for mocking in tests.
  virtual void StartPrefetch(const AutocompleteInput& input);

  // Cancels the current query, ensuring most future updates won't fire
  // notifications. If new matches have come in since the most recent
  // notification was fired, they may be discarded. See
  // `AutocompleteProvider::Stop()` & `AutocompleteStopReason`.
  void Stop(AutocompleteStopReason stop_reason);

  // Asks the relevant provider to delete |match|, and ensures observers are
  // notified of resulting changes immediately.  This should only be called when
  // no query is running.
  void DeleteMatch(const AutocompleteMatch& match);

  // Asks the relevant provider to partially delete match, and ensures observers
  // are notified of resulting changes immediately.  This should only be called
  // when no query is running.
  // Calling this method does not imply removal of the AutocompleteMatch.
  // |element_index| parameter specifies which part of the match should be
  // deleted. For cases where the entire AutocompleteMatch should be removed,
  // please see |DeleteMatch| method.
  void DeleteMatchElement(const AutocompleteMatch& match, size_t element_index);

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  // Called when an omnibox event log entry is generated. Populates
  // `log.provider_info` with diagnostic information about the status of various
  // providers and `log.features_triggered[_in_session]` with triggered
  // features.
  void AddProviderAndTriggeringLogs(OmniboxLog* logs) const;

  // Called when a new omnibox session starts.
  // We start a new session when the user first begins modifying the omnibox
  // content; see |OmniboxEditModel::user_input_in_progress_|.
  void ResetSession();

  // Updates the destination URL for the given match with the final searchbox
  // stats parameter using additional information otherwise not available at
  // initial construction time iff the provider's TemplateURL supports searchbox
  // stats.
  // This method should be called right before the user navigates to the match.
  void UpdateMatchDestinationURLWithAdditionalSearchboxStats(
      base::TimeDelta query_formulation_time,
      AutocompleteMatch* match) const;

  void UpdateSearchTermsArgsWithAdditionalSearchboxStats(
      base::TimeDelta query_formulation_time,
      TemplateURLRef::SearchTermsArgs& search_terms_args) const;

  // Constructs and sets the final destination URL on the given match.
  void SetMatchDestinationURL(AutocompleteMatch* match) const;

  HistoryURLProvider* history_url_provider() const {
    return history_url_provider_;
  }
  KeywordProvider* keyword_provider() const { return keyword_provider_; }
  UnscopedExtensionProvider* unscoped_extension_provider() const {
    return unscoped_extension_provider_;
  }
  SearchProvider* search_provider() const { return search_provider_; }
  ClipboardProvider* clipboard_provider() const { return clipboard_provider_; }
  VoiceSuggestProvider* voice_suggest_provider() const {
    return voice_suggest_provider_;
  }
  OpenTabProvider* open_tab_provider() const { return open_tab_provider_; }
  ContextualSearchProvider* contextual_search_provider() const {
    return contextual_search_provider_;
  }

  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return published_result_; }
  // Groups `published_result_` by search vs URL.
  // See also `AutocompleteResult::GroupSuggestionsBySearchVsURL()`.
  virtual void GroupSuggestionsBySearchVsURL(size_t begin, size_t end);
  bool done() const {
    return last_update_type_ == UpdateType::kNone ||
           last_update_type_ == UpdateType::kSyncPassOnly ||
           last_update_type_ == UpdateType::kLastAsyncPass ||
           last_update_type_ == UpdateType::kStop ||
           last_update_type_ == UpdateType::kMatchDeletion;
  }
  UpdateType last_update_type() const { return last_update_type_; }
  const Providers& providers() const { return providers_; }

  // Returns whether the given provider should be ran based on whether we're in
  // keyword mode and which keyword we're searching. Currently runs all enabled
  // providers unless in a Starter Pack scope, except for the @tabs scope.
  bool ShouldRunProvider(AutocompleteProvider* provider) const;

  const base::TimeTicks& last_time_default_match_changed() const {
    return last_time_default_match_changed_;
  }

  // Returns the AutocompleteProviderClient owned by the controller.
  AutocompleteProviderClient* autocomplete_provider_client() const {
    return provider_client_.get();
  }

  // This is a deprecated method of injecting an externally sourced
  // match into the result set, currently still needed only by iOS.
  size_t InjectAdHocMatch(AutocompleteMatch match);

#if BUILDFLAG(IS_IOS)
  // Sets the position of the omnibox when it's in steady state (unfocused).
  // Only used on iOS for logging purposes.
  virtual void SetSteadyStateOmniboxPosition(
      metrics::OmniboxEventProto::OmniboxPosition position);
#endif

 private:
  friend class FakeAutocompleteController;
  friend class AutocompleteProviderTest;
  friend class OmniboxRowGroupedViewBrowserTest;
  friend class OmniboxSuggestionButtonRowBrowserTest;
  friend class ZeroSuggestPrefetchTabHelperBrowserTest;
  friend class OmniboxEditModelPopupTest;
  friend class OmniboxMetricsTest;
  friend class OmniboxSearchAggregatorTest;
  friend class extensions::UnscopedOmniboxApiTest;
  friend class SearchPreloadResponseController;
#if BUILDFLAG(IS_IOS)
  friend class OmniboxInttestAutocompleteController;
#endif
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           FilterMatchesForInstantKeywordWithBareAt);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           NoActionsAttachedToLensSearchboxMatches);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           NoActionsAttachedToNtpComposeboxMatches);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           ContextualQueryAppendsSearchboxStats);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           ContextualSearchActionAttachedPageKeywordMode);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           ContextualSearchActionAttachedInZeroSuggest);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteControllerTest,
                           AttachContextualSearchOpenLensActionToMatches);
  FRIEND_TEST_ALL_PREFIXES(
      AutocompleteControllerTest,
      ContextualSearchOpenLensActionAttachedPageKeywordMode);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest,
                           RedundantKeywordsIgnoredInResult);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, UpdateSearchboxStats);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderPrefetchTest,
                           SupportedProvider_NonPrefetch);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderPrefetchTest,
                           SupportedProvider_Prefetch);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderPrefetchTest,
                           SupportedProvider_OngoingNonPrefetch);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderPrefetchTest,
                           UnsupportedProvider_Prefetch);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupSuggestionGroupHeadersTest,
                           ShowSuggestionGroupHeadersByPageContext);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, EmitAccessibilityEvents);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest,
                           EmitAccessibilityEventsOnButtonFocusHint);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, DeleteSuggestion);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewTest, DoesNotUpdateAutocompleteOnBlur);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, AccessiblePopup);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, MaintainCursorAfterFocusCycle);
#if BUILDFLAG(IS_WIN)
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsUIATest, AccessibleOmnibox);
#endif
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest,
                           EmitSelectedChildrenChangedAccessibilityEvent);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest,
                           AccessibleActivedescendantId);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest,
                           AccessibleSelectionOnResultSelection);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, AccessibleResultName);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, RealboxUpdatesEditModelInput);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewPopupTest, GetIcon_IconUrl);

  // A minimal representation of the previous `AutocompleteResult`. Used by
  // `UpdateResult()`'s helper methods.
  struct OldResult {
    OldResult(UpdateType update_type,
              const AutocompleteInput& input,
              AutocompleteResult* result);
    ~OldResult();

    std::optional<AutocompleteMatch> last_default_match;
    std::u16string last_default_associated_keyword;
    AutocompleteResult matches_to_transfer;
    std::optional<AutocompleteMatch> default_match_to_preserve;
  };

  // Helpers called by the constructor. These initialize the specified providers
  // and add them `providers_`. Split into 2 methods to avoid accidentally
  // adding providers in the wrong order (async providers should be added first
  // so that they run first).
  void InitializeAsyncProviders(int provider_types);
  void InitializeSyncProviders(int provider_types);

  // Updates `internal_result_` to reflect the current provider state and fires
  // notifications.
  // TODO(crbug.com/364303536): `allow_post_done_updates` allows some exceptions
  //   in the DCHECKs that verify the order of `update_type`s used in
  //   consecutive `UpdateResult()` calls makes sense. It's a temporary fix for
  //   allowing history embedding answers to `UpdateResults()` after
  //   `stop_timer_` has fired.
  void UpdateResult(UpdateType update_type,
                    bool allow_post_done_updates = false);

  // `UpdateResult()` helper. Aggregates matches from `providers_` into
  // `internal_result_`.
  void AggregateNewMatches();

  // `UpdateResult()` helper. Gets ML scores for eligible matches in
  // `internal_result_` and then sorts `internal_result_` accordingly.
  void MlRerank(OldResult& old_result);

  // `UpdateResult()` helper. Calls multiple other helpers (see implementation).
  void PostProcessMatches();

  // `UpdateResult()` helper. Returns whether the default match changed.
  bool CheckWhetherDefaultMatchChanged(
      std::optional<AutocompleteMatch> last_default_match,
      const std::u16string& last_default_associated_keyword);

  // Attaches actions to matches: pedals, history clusters, tab switch, etc.
  void AttachActions();

  // Updates `result` to populate each match's `associated_keyword` if that
  // match can show a keyword hint. `result` should be sorted by relevance
  // before this is called.
  void UpdateAssociatedKeywords(AutocompleteResult* result);

  // For each group of contiguous matches from the same TemplateURL, show the
  // provider name as a description on the first match in the group. Starter
  // Pack matches show their URLs as descriptions instead of the provider name.
  void UpdateKeywordDescriptions(AutocompleteResult* result);

  // For each AutocompleteMatch in `result`, updates the searchbox stats iff the
  // provider's TemplateURL supports it.
  void UpdateSearchboxStats(AutocompleteResult* result);

  // For each AutocompleteMatch in `result`, updates the "shown in session" data
  // that's needed in order to ensure proper client-side metrics logging.
  void UpdateShownInSession(AutocompleteResult* result);

  // Update the tail suggestions' `tail_suggest_common_prefix`.
  void UpdateTailSuggestPrefix(AutocompleteResult* result);

  // Calls AutocompleteController::Observer::OnResultChanged() and if done sends
  // AUTOCOMPLETE_CONTROLLER_RESULT_READY.
  void NotifyChanged();

  // Invokes `NotifyChanged()` through `notify_changed_debouncer_`.
  void RequestNotifyChanged(bool notify_default_match, bool delayed);

  // Cancels any pending `NotifyChanged()` invocation through
  // `notify_changed_debouncer_`.
  void CancelNotifyChangedRequest();

  // Returns which of the providers that should run are done.
  enum class ProviderDoneState {
    kNotDone,
    kAllExceptDocDone,
    kAllDone,
  };
  ProviderDoneState GetProviderDoneState();

  // Starts |expire_timer_|.
  void StartExpireTimer();

  // Starts |stop_timer_|.
  void StartStopTimer();

  // Helper function for `Stop()`. Called specifically when the stop timer
  // expires.
  void OnStopTimerTriggered();

  // MemoryDumpProvider:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Runs the batch scoring for all the eligible matches in `results_.matches_`.
  void RunBatchUrlScoringModel(OldResult& old_result);
  void RunBatchUrlScoringModelMappedSearchBlending(OldResult& old_result);
  void RunBatchUrlScoringModelPiecewiseMappedSearchBlending(
      OldResult& old_result);
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  // Constructs a destination URL from supplied search terms args.
  // TODO(crbug.com/40257536): look for a way to dissolve this function into
  // direct application where it's needed.
  GURL ComputeURLFromSearchTermsArgs(
      const TemplateURL* template_url,
      const TemplateURLRef::SearchTermsArgs& args) const;

  // Ablates company entity image when the first suggestion is a historical URL
  // and its domain is equal to an entity suggestion's domain.
  void MaybeRemoveCompanyEntityImages(AutocompleteResult* result);

  // May remove actions from default suggestion to avoid interference with
  // keyword mode refresh interaction. May clear some match text that is
  // repeated across multiple consecutive matches.
  void MaybeCleanSuggestionsForKeywordMode(const AutocompleteInput& input,
                                           AutocompleteResult* result);

  // Removes promotional IPH suggestions if `result` contains toolbelt. Does not
  // remove disclaimer IPHs.
  void MaybeCleanIphSuggestions(AutocompleteResult* result);

  // Get the experiment stats v2 entry for the omnibox position. Used on iOS.
  const omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2
  GetOmniboxPositionExperimentStatsV2() const;

  base::ObserverList<Observer> observers_;

  // The client passed to the providers.
  const std::unique_ptr<AutocompleteProviderClient> provider_client_;

  // A list of all providers.
  Providers providers_;
  raw_ptr<BookmarkProvider> bookmark_provider_ = nullptr;
  raw_ptr<HistoryQuickProvider> history_quick_provider_ = nullptr;
  raw_ptr<DocumentProvider> document_provider_ = nullptr;
  raw_ptr<HistoryURLProvider> history_url_provider_ = nullptr;
  raw_ptr<KeywordProvider> keyword_provider_ = nullptr;
  raw_ptr<UnscopedExtensionProvider> unscoped_extension_provider_ = nullptr;
  raw_ptr<SearchProvider> search_provider_ = nullptr;
  raw_ptr<ZeroSuggestProvider> zero_suggest_provider_ = nullptr;
  raw_ptr<OnDeviceHeadProvider> on_device_head_provider_ = nullptr;
  raw_ptr<ClipboardProvider> clipboard_provider_ = nullptr;
  raw_ptr<VoiceSuggestProvider> voice_suggest_provider_ = nullptr;
  raw_ptr<HistoryFuzzyProvider> history_fuzzy_provider_ = nullptr;
  raw_ptr<OpenTabProvider> open_tab_provider_ = nullptr;
  raw_ptr<TabGroupProvider> tab_group_provider_ = nullptr;
  raw_ptr<FeaturedSearchProvider> featured_search_provider_ = nullptr;
  raw_ptr<ContextualSearchProvider> contextual_search_provider_ = nullptr;

  // A vector of scoring signals annotators for URL suggestions.
  // Unlike the other existing annotators (e.g., pedals and keywords), these
  // signal annotations should be done before the sort and cull pass.
  std::vector<std::unique_ptr<AutocompleteScoringSignalsAnnotator>>
      url_scoring_signals_annotators_;

  // Input passed to Start.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult internal_result_;

  // A snapshot of `internal_result_` when `NotifyChanged()` is called. Because
  // it's debounced, `internal_result_` may change without invoking
  // `NotifyChanged()`. `published_result_` ensures observers get a stable
  // result.
  AutocompleteResult published_result_;

  // Used for logging the changes between updates.
  std::vector<AutocompleteResult::MatchDedupComparator>
      last_result_for_logging_;

  // The most recent time the default match (inline match) changed.  This may
  // be earlier than the most recent keystroke if the recent keystrokes didn't
  // change the suggested match in the omnibox.  (For instance, if
  // a user typed "mail.goog" and the match https://mail.google.com/ was
  // the destination match ever since the user typed "ma" then this is
  // the time that URL first appeared as the default match.)  This may
  // also be more recent than the last keystroke if there was an
  // asynchronous provider that returned and changed the default
  // match.  See UpdateResult() for details on when we consider a
  // match to have changed.
  // This is very similar to `metrics_.last_default_change_time_`, but whereas
  // that is reset on `::Start()`, this is not.
  base::TimeTicks last_time_default_match_changed_;

  // Timer used to remove any matches copied from the last result. When run
  // invokes |ExpireCopiedEntries|.
  base::OneShotTimer expire_timer_;

  // Timer used to tell the providers to Stop() searching for matches.
  base::OneShotTimer stop_timer_;

  // Debouncer to avoid invoking `NotifyChange()` after updating results in
  // quick succession. The last call, i.e. when all providers complete and
  // `done_` is set true; and the 1st call, i.e. the sync update, are immune to
  // this restriction. Calls not succeeding a result update (i.e. a call from
  // closing the popup) bypass the delay as well.
  AutocompleteProviderDebouncer notify_changed_debouncer_{false, 200};

  // Tracks if any delayed `RequestNotifyChanged()` call since the last
  // `NotifyChanged()` call changed the default match. Otherwise, if there have
  // been 2 delayed calls, the 1st having changed the default, the latter not,
  // `NotifyChanged()` couldn't know of the former.
  bool notify_changed_default_match_ = false;

  // Represents the reason of the last `UpdateResult()` call.
  UpdateType last_update_type_ = UpdateType::kNone;

  // Logs stability and timing metrics for updates.
  AutocompleteControllerMetrics metrics_{*this};

  // True if the signal predicting a likely search has already been sent to the
  // service worker context during the current input session. False on
  // controller creation and after |ResetSession| is called.
  bool search_service_worker_signal_sent_ = false;

  const raw_ptr<TemplateURLService> template_url_service_;

  const raw_ptr<OmniboxTriggeredFeatureService> triggered_feature_service_;

  // The preferred steady state (unfocused) omnibox position.
  metrics::OmniboxEventProto::OmniboxPosition steady_state_omnibox_position_;

  // Configures autocomplete provider for different embedders.
  // TODO(crbug.com/455133849 & crbug.com/455132352): Make `const` after
  //   removing `OmniboxFieldTrial::GetDisabledProviderTypes()` &
  //   `SetStartStopTimerDurationForTesting()`.
  AutocompleteControllerConfig config_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_
