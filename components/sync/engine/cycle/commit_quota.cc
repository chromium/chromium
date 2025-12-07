// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/commit_quota.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/time/time.h"

namespace syncer {

CommitQuota::CommitQuota(int max_tokens, base::TimeDelta refill_interval)
    : max_tokens_(max_tokens),
      refill_interval_(refill_interval),
      tokens_(max_tokens),
      last_refilled_(base::TimeTicks::Now()) {
  DCHECK_GT(max_tokens_, 0);
  DCHECK_GT(refill_interval_, base::TimeDelta());
}

CommitQuota::CommitQuota(const CommitQuota&) = default;
CommitQuota& CommitQuota::operator=(const CommitQuota&) = default;
CommitQuota::CommitQuota(CommitQuota&&) = default;
CommitQuota& CommitQuota::operator=(CommitQuota&&) = default;

CommitQuota::~CommitQuota() = default;

void CommitQuota::SetParams(int new_max_tokens,
                            base::TimeDelta refill_interval) {
  DCHECK_GT(new_max_tokens, 0);
  DCHECK_GT(refill_interval, base::TimeDelta());
  if (max_tokens_ > new_max_tokens) {
    // Cap current tokens by the newly lowered `new_max_tokens` count.
    tokens_ = std::min(tokens_, new_max_tokens);
  } else if (max_tokens_ < new_max_tokens) {
    // Raise current tokens by the newly raised `max_tokens` count. This
    // respects the current level of consumed tokens and does not automatically
    // reset the token count to `max_tokens`.
    const int additional_tokens = new_max_tokens - max_tokens_;
    tokens_ += additional_tokens;
  }
  max_tokens_ = new_max_tokens;
  refill_interval_ = refill_interval;
  DCHECK_GE(tokens_, 0);
  DCHECK_LE(tokens_, max_tokens_);
}

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
