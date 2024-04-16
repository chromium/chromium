// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace safe_browsing {

SafeBrowsingTokenFetchTracker::SafeBrowsingTokenFetchTracker()
    : weak_ptr_factory_(this) {
}

SafeBrowsingTokenFetchTracker::~SafeBrowsingTokenFetchTracker() {
  for (auto& id_and_callback : callbacks_) {
    std::move(id_and_callback.second).Run(std::string());
  }
}

int SafeBrowsingTokenFetchTracker::StartTrackingTokenFetch(
    SafeBrowsingTokenFetcher::Callback on_token_fetched_callback,
    OnTokenFetchTimeoutCallback on_token_fetch_timeout_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int request_id = requests_sent_;
  requests_sent_++;
  callbacks_[request_id] = std::move(on_token_fetched_callback);
  // TODO(crbug.com/40808768): Use base::OneShotTimer here to enabling
  // cancelling tracking of timeouts when requests complete. The implementation
  // of OnTokenFetchTimeout can then be correspondingly simplified.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingTokenFetchTracker::OnTokenFetchTimeout,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(on_token_fetch_timeout_callback)),
      base::Milliseconds(kTokenFetchTimeoutDelayFromMilliseconds));

  return request_id;
}

void SafeBrowsingTokenFetchTracker::OnTokenFetchComplete(
    int request_id,
    std::string access_token) {
  Finish(request_id, access_token);
}

void SafeBrowsingTokenFetchTracker::OnTokenFetchTimeout(
    int request_id,
    OnTokenFetchTimeoutCallback on_token_fetch_timeout_callback) {
  // The request might have already completed, in which case there
  // is nothing to be done here.
  if (!callbacks_.contains(request_id))
    return;

  Finish(request_id, std::string());

  std::move(on_token_fetch_timeout_callback).Run(request_id);
}

void SafeBrowsingTokenFetchTracker::Finish(int request_id,
                                           const std::string& access_token) {
  if (!callbacks_.contains(request_id))
    return;

  // Remove the callback from the map before running it so that this object
  // doesn't try to run this callback again if deleted from within the
  // callback.
  auto callback = std::move(callbacks_[request_id]);
  callbacks_.erase(request_id);

  std::move(callback).Run(access_token);

  // NOTE: Invoking the callback might have resulted in the synchronous
  // destruction of this object, so there is nothing safe to do here but return.
}

}  // namespace safe_browsing
