// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/ntp_snippets/content_suggestions_provider.h"

namespace ntp_snippets {

class RemoteSuggestionsFetcher;

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
class RemoteSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  // Callback to notify with the result of a fetch.
  using FetchStatusCallback = base::OnceCallback<void(Status status_code)>;

  ~RemoteSuggestionsProvider() override;

  // Fetches suggestions from the server for all remote categories and replaces
  // old suggestions by the new ones. The request to the server is performed as
  // an background request. Background requests are used for actions not
  // triggered by the user and have lower priority on the server. After the
  // fetch finished, the provided |callback| will be triggered with the status
  // of the fetch (unless nullptr). If the provider is not ready(), the fetch
  // fails and the callback gets immediately called with an error message.
  virtual void RefetchInTheBackground(FetchStatusCallback callback) = 0;

  // Refetches the suggestions in a state ready for display. Similar to
  // |RefetchInTheBackground| above, but observers will be notified about the
  // ongoing refetch and may be notified with old suggestions if the fetch fails
  // or does not finish before timeout.
  virtual void RefetchWhileDisplaying(FetchStatusCallback callback) = 0;

  virtual const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const = 0;

  // Get the url with favicon for the suggestion.
  virtual GURL GetUrlWithFavicon(
      const ContentSuggestion::ID& suggestion_id) const = 0;

  // Whether the provider is explicity disabled.
  virtual bool IsDisabled() const = 0;

  // Whether the provider is ready to fetch suggestions. While the provider is
  // not ready, all operations on it will fail or get ignored.
  virtual bool ready() const = 0;

 protected:
  RemoteSuggestionsProvider(Observer* observer);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_
