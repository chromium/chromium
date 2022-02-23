// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/commit_quota.h"

#include <algorithm>

#include "base/time/time.h"

namespace syncer {

CommitQuota::CommitQuota(int initial_tokens, base::TimeDelta refill_interval)
    : max_tokens_(initial_tokens),
      refill_interval_(refill_interval),
      tokens_(initial_tokens),
      last_refilled_(base::TimeTicks::Now()) {
  DCHECK_GT(refill_interval, base::TimeDelta());
}

CommitQuota::~CommitQuota() = default;

bool CommitQuota::HasTokensAvailable() {
  RefillTokens();
  return tokens_ > 0;
}

void CommitQuota::ConsumeToken() {
  RefillTokens();
  if (tokens_ > 0) {
    --tokens_;
    return;
  }

  // When no token is available to consume, at least push away the next refill
  // time to make sure any `HasTokensAvailable()` calls in the next
  // `refill_interval_` period return false.
  last_refilled_ = base::TimeTicks::Now();
}

void CommitQuota::RefillTokens() {
  const base::TimeDelta since_last_refilled =
      base::TimeTicks::Now() - last_refilled_;
  const int new_tokens = since_last_refilled / refill_interval_;
  if (new_tokens == 0) {
    return;
  }

  tokens_ = std::min(tokens_ + new_tokens, max_tokens_);
  last_refilled_ += new_tokens * refill_interval_;
  DCHECK_GE(tokens_, 0);
}

}  // namespace syncer
