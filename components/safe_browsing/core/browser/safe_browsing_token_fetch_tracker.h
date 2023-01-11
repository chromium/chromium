// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCH_TRACKER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCH_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"

namespace safe_browsing {

// Exposed for unittests.
#if BUILDFLAG(IS_ANDROID)
constexpr int kTokenFetchTimeoutDelayFromMilliseconds = 50;
#else
constexpr int kTokenFetchTimeoutDelayFromMilliseconds = 1000;
#endif

// Helper class for use by implementations of SafeBrowsingTokenFetcher:
// tracks a set of outstanding access token fetches, timing out a fetch after a
// given delay.
class SafeBrowsingTokenFetchTracker {
 public:
  using OnTokenFetchTimeoutCallback = base::OnceCallback<void(int request_id)>;

  SafeBrowsingTokenFetchTracker();
  ~SafeBrowsingTokenFetchTracker();

  // Should be invoked when a safe browsing access token fetch is started. Takes
  // in the callback that the client passed to SafeBrowsingTokenFetcher::Start()
  // as well as a callback via which the SafeBrowsingTokenFetcher implementation
  // is informed of token fetch timeouts. Returns the request ID associated with
  // the fetch. If the access token is fetched before the timeout is invoked,
  // the SafeBrowsingTokenFetcher implementation should invoke
  // OnTokenFetchComplete(), in which case this object will invoke
  // |on_token_fetched_callback| with the given access token. If the timeout
  // occurs, this object will invoke |on_token_fetched_callback| with an empty
  // token and invoke |on_token_fetch_timeout_callback| so that the
  // SafeBrowsingTokenFetcher implementation can clean up any associated state.
  int StartTrackingTokenFetch(
      SafeBrowsingTokenFetcher::Callback on_token_fetched_callback,
      OnTokenFetchTimeoutCallback on_token_fetch_timeout_callback);

  // Should be invoked when an access token fetch has completed.
  void OnTokenFetchComplete(int request_id, std::string access_token);

 private:
  void OnTokenFetchTimeout(
      int request_id,
      OnTokenFetchTimeoutCallback on_token_fetch_timeout_callback);
  void Finish(int request_id, const std::string& access_token);

  SEQUENCE_CHECKER(sequence_checker_);

  // The count of requests sent. This is used as an ID for requests.
  int requests_sent_ = 0;

  // Active callbacks, keyed by ID.
  base::flat_map<int, SafeBrowsingTokenFetcher::Callback> callbacks_;

  base::WeakPtrFactory<SafeBrowsingTokenFetchTracker> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_TOKEN_FETCH_TRACKER_H_
