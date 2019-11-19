// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"

class AutocompleteControllerDelegate;
class DocumentProvider;
class HistoryURLProvider;
class KeywordProvider;
class SearchProvider;
class TemplateURLService;
class ZeroSuggestProvider;
class OnDeviceHeadProvider;

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
 public:
  typedef std::vector<scoped_refptr<AutocompleteProvider> > Providers;

  // |provider_types| is a bitmap containing AutocompleteProvider::Type values
  // that will (potentially, depending on platform, flags, etc.) be
  // instantiated. |provider_client| is passed to all those providers, and
  // is used to get access to the template URL service. |delegate| is a
  // proxy for UI elements which need to be notified when the results get
  // updated.
  AutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      AutocompleteControllerDelegate* delegate,
      int provider_types);
  ~AutocompleteController() override;

  // Starts an autocomplete query, which continues until all providers are
  // done or the query is Stop()ed.  It is safe to Start() a new query without
  // Stop()ing the previous one.
  //
  // See AutocompleteInput::AutocompleteInput(...) for more details regarding
  // |input| params.
  //
  // The controller calls AutocompleteControllerDelegate::OnResultChanged() from
  // inside this call at least once. If matches are available later on that
  // result in changing the result set the delegate is notified again. When the
  // controller is done the notification AUTOCOMPLETE_CONTROLLER_RESULT_READY is
  // sent.
  void Start(const AutocompleteInput& input);

  // Cancels the current query, ensuring there will be no future notifications
  // fired.  If new matches have come in since the most recent notification was
  // fired, they will be discarded.
  //
  // If |clear_result| is true, the controller will also erase the result set.
  void Stop(bool clear_result);

  // Asks the relevant provider to delete |match|, and ensures observers are
  // notified of resulting changes immediately.  This should only be called when
  // no query is running.
  void DeleteMatch(const AutocompleteMatch& match);

  // Removes any entries that were copied from the last result. This is used by
  // the popup to ensure it's not showing an out-of-date query.
  void ExpireCopiedEntries();

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  // Called when an omnibox event log entry is generated.
  // Populates provider_info with diagnostic information about the status
  // of various providers.  In turn, calls
  // AutocompleteProvider::AddProviderInfo() so each provider can add
  // provider-specific information, information we want to log for a particular
  // provider but not others.
  void AddProvidersInfo(ProvidersInfo* provider_info) const;

  // Called when a new omnibox session starts.
  // We start a new session when the user first begins modifying the omnibox
  // content; see |OmniboxEditModel::user_input_in_progress_|.
  void ResetSession();

  // Constructs the final destination URL for a given match using additional
  // parameters otherwise not available at initial construction time.  This
  // method should be called from OmniboxEditModel::OpenMatch() before the user
  // navigates to the selected match.
  void UpdateMatchDestinationURLWithQueryFormulationTime(
      base::TimeDelta query_formulation_time,
      AutocompleteMatch* match) const;

  // Constructs the final destination URL for a given match using additional
  // parameters otherwise not available at initial construction time.
  void UpdateMatchDestinationURL(
      const TemplateURLRef::SearchTermsArgs& search_terms_args,
      AutocompleteMatch* match) const;

  // Prepend missing tail suggestion prefixes in results, if present.
  void InlineTailPrefixes();

  HistoryURLProvider* history_url_provider() const {
    return history_url_provider_;
  }
  KeywordProvider* keyword_provider() const { return keyword_provider_; }
  SearchProvider* search_provider() const { return search_provider_; }

  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return result_; }
  bool done() const { return done_; }
  const Providers& providers() const { return providers_; }

  const base::TimeTicks& last_time_default_match_changed() const {
    return last_time_default_match_changed_;
  }

 private:
  friend class AutocompleteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest,
                           RedundantKeywordsIgnoredInResult);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, UpdateAssistedQueryStats);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupContentsViewTest,
                           EmitTextChangedAccessibilityEvent);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewTest, DoesNotUpdateAutocompleteOnBlur);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, AccessiblePopup);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, MaintainCursorAfterFocusCycle);
#if defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsUIATest, AccessibleOmnibox);
#endif  // OS_WIN
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupModelTest, SetSelectedLine);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupModelTest,
                           SetSelectedLineWithNoDefaultMatches);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupModelTest, TestFocusFixing);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupModelTest, PopupPositionChanging);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupContentsViewTest,
                           EmitSelectedChildrenChangedAccessibilityEvent);

  // Updates |result_| to reflect the current provider state and fires
  // notifications.  If |regenerate_result| then we clear the result
  // so when we incorporate the current provider state we end up
  // implicitly removing all expired matches.  (Normally we allow
  // matches from the previous result set carry over.  These stale
  // results may outrank legitimate matches from the current result
  // set.  Sometimes we just want the current matches; the easier way
  // to do this is to throw everything out and reconstruct the result
  // set from the providers' current data.)
  // If |force_notify_default_match_changed|, we tell NotifyChanged
  // the default match has changed even if it hasn't.  This is
  // necessary in some cases; for instance, if the user typed a new
  // character, the edit model needs to repaint (highlighting changed)
  // even if the default match didn't change.
  void UpdateResult(bool regenerate_result,
                    bool force_notify_default_match_changed);

  // Updates |result| to populate each match's |associated_keyword| if that
  // match can show a keyword hint.  |result| should be sorted by
  // relevance before this is called.
  void UpdateAssociatedKeywords(AutocompleteResult* result);

  // For each group of contiguous matches from the same TemplateURL, show the
  // provider name as a description on the first match in the group.
  void UpdateKeywordDescriptions(AutocompleteResult* result);

  // For each AutocompleteMatch returned by SearchProvider, updates the
  // destination_url iff the provider's TemplateURL supports assisted query
  // stats.
  void UpdateAssistedQueryStats(AutocompleteResult* result);

  // Calls AutocompleteControllerDelegate::OnResultChanged() and if done sends
  // AUTOCOMPLETE_CONTROLLER_RESULT_READY.
  void NotifyChanged(bool notify_default_match);

  // Updates |done_| to be accurate with respect to current providers' statuses.
  void CheckIfDone();

  // Starts |expire_timer_|.
  void StartExpireTimer();

  // Starts |stop_timer_|.
  void StartStopTimer();

  // Helper function for Stop().  |due_to_user_inactivity| means this call was
  // triggered by a user's idleness, i.e., not an explicit user action.
  void StopHelper(bool clear_result,
                  bool due_to_user_inactivity);

  // Helper for UpdateKeywordDescriptions(). Returns whether curbing the keyword
  // descriptions is enabled, and whether there is enough input to guarantee
  // that the Omnibox is in keyword mode.
  bool ShouldCurbKeywordDescriptions(const base::string16& keyword);

  // MemoryDumpProvider:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  AutocompleteControllerDelegate* delegate_;

  // The client passed to the providers.
  std::unique_ptr<AutocompleteProviderClient> provider_client_;

  // A list of all providers.
  Providers providers_;

  DocumentProvider* document_provider_;

  HistoryURLProvider* history_url_provider_;

  KeywordProvider* keyword_provider_;

  SearchProvider* search_provider_;

  ZeroSuggestProvider* zero_suggest_provider_;

  OnDeviceHeadProvider* on_device_head_provider_;

  // Input passed to Start.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult result_;

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
  base::TimeTicks last_time_default_match_changed_;

  // Timer used to remove any matches copied from the last result. When run
  // invokes |ExpireCopiedEntries|.
  base::OneShotTimer expire_timer_;

  // Timer used to tell the providers to Stop() searching for matches.
  base::OneShotTimer stop_timer_;

  // Amount of time (in ms) between when the user stops typing and
  // when we send Stop() to every provider.  This is intended to avoid
  // the disruptive effect of belated omnibox updates, updates that
  // come after the user has had to time to read the whole dropdown
  // and doesn't expect it to change.
  const base::TimeDelta stop_timer_duration_;

  // True if a query is not currently running.
  bool done_;

  // Are we in Start()? This is used to avoid updating |result_| and sending
  // notifications until Start() has been invoked on all providers. When this
  // boolean is true, we are definitely within the synchronous pass.
  bool in_start_;

  // Indicate whether it is the first query since startup.
  bool first_query_;

  // True if the signal predicting a likely search has already been sent to the
  // service worker context during the current input session. False on
  // controller creation and after |ResetSession| is called.
  bool search_service_worker_signal_sent_;

  TemplateURLService* template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_H_
