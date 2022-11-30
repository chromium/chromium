// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ntp_snippets {

// Returns the appropriate API endpoint for the fetcher, in consideration of
// variation parameters.
GURL GetFetchEndpoint();

// Fetches suggestion data for the NTP from the server.
class RemoteSuggestionsFetcher {
 public:
  using OptionalFetchedCategories = absl::optional<FetchedCategoriesVector>;
  using SnippetsAvailableCallback =
      base::OnceCallback<void(Status status,
                              OptionalFetchedCategories fetched_categories)>;

  virtual ~RemoteSuggestionsFetcher();

  // Initiates a fetch from the server. When done (successfully or not), calls
  // the callback.
  //
  // If an ongoing fetch exists, both fetches won't influence each other (i.e.
  // every callback will be called exactly once).
  virtual void FetchSnippets(const RequestParams& params,
                             SnippetsAvailableCallback callback) = 0;

  // Debug string representing the status/result of the last fetch attempt.
  virtual const std::string& GetLastStatusForDebugging() const = 0;

  // Returns the last JSON fetched from the server.
  virtual const std::string& GetLastJsonForDebugging() const = 0;

  // Returns whether the last fetch was authenticated.
  virtual bool WasLastFetchAuthenticatedForDebugging() const = 0;

  // Returns the URL endpoint used by the fetcher.
  virtual const GURL& GetFetchUrlForDebugging() const = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_FETCHER_H_
