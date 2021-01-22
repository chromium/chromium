// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/thread_utils.h"

namespace safe_browsing {

SafeBrowsingTokenFetchTracker::SafeBrowsingTokenFetchTracker()
    : weak_ptr_factory_(this) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
}

SafeBrowsingTokenFetchTracker::~SafeBrowsingTokenFetchTracker() {
  for (auto& id_and_callback : callbacks_) {
    std::move(id_and_callback.second).Run(std::string());
  }
}

int SafeBrowsingTokenFetchTracker::StartTrackingTokenFetch(
    SafeBrowsingTokenFetcher::Callback on_token_fetched_callback,
    OnTokenFetchTimeoutCallback on_token_fetch_timeout_callback) {
  DCHECK(CurrentlyOnThread(ThreadID::UI));
  const int request_id = requests_sent_;
  requests_sent_++;
  callbacks_[request_id] = std::move(on_token_fetched_callback);
  base::PostDelayedTask(
      FROM_HERE, CreateTaskTraits(ThreadID::UI),
      base::BindOnce(&SafeBrowsingTokenFetchTracker::OnTokenFetchTimeout,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::move(on_token_fetch_timeout_callback)),
      base::TimeDelta::FromMilliseconds(
          kTokenFetchTimeoutDelayFromMilliseconds));

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
}

}  // namespace safe_browsing
