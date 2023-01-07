// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_

namespace ntp_snippets {

class RemoteSuggestionsProvider;
struct Status;

// The scheduler for background fetching of remote suggestions has two callers:
//  a) The actual provider that implements the fetching informs the scheduler
//     about relevant events and changes in its state.
//  b) External parties (such as the UI) that need to inform the scheduler about
//     their events.
class RemoteSuggestionsScheduler {
 public:
  virtual ~RemoteSuggestionsScheduler() = default;

  // Set the provider that performs background fetching. Should be only called
  // by the factory.
  virtual void SetProvider(RemoteSuggestionsProvider* provider) = 0;

  // ***** Internal triggers to consider fetching content suggestions. *****

  // Called whenever the remote suggestions provider becomes active (on startup,
  // or later on).
  virtual void OnProviderActivated() = 0;

  // Called whenever the remote suggestions provider becomes inactive (on
  // startup, or later on).
  virtual void OnProviderDeactivated() = 0;

  // Called whenever the remote suggestions provider clears all suggestions.
  virtual void OnSuggestionsCleared() = 0;

  // Called whenever the remote suggestions provider clears all suggestions
  // because history gets cleared (and we must not show them any more).
  virtual void OnHistoryCleared() = 0;

  // Returns true if quota is available for another request.
  virtual bool AcquireQuotaForInteractiveFetch() = 0;

  // Called whenever the remote suggestions provider finishes an interactive
  // fetch (with provided |fetch_status|).
  virtual void OnInteractiveFetchFinished(Status fetch_status) = 0;

  // ***** External triggers to consider fetching content suggestions. *****

  // Called whenever chrome is started warm or the user switches to Chrome.
  virtual void OnBrowserForegrounded() = 0;

  // Called whenever chrome is cold started.
  // To keep start ups fast, defer any work possible.
  virtual void OnBrowserColdStart() = 0;

  // Called whenever a new suggestions surface is opened. This may be called on
  // cold starts. So to keep start ups fast, defer heavy work for cold starts.
  virtual void OnSuggestionsSurfaceOpened() = 0;

  // Called by PersistentScheduler implementation whenever it wakes up according
  // to its schedule. Avoid heavy work, Chrome may be running in the background.
  virtual void OnPersistentSchedulerWakeUp() = 0;

  virtual void OnBrowserUpgraded() = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
