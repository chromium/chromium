// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_

#include "base/time/time.h"

namespace syncer {

// Tracks quota for commits with `max_tokens` being also the initial token
// count. A token is refilled every `refill_interval` if below the maximum token
// count. Tokens are consumed one by one by `ConsumeToken()` until they reach
// the zero. When having zero tokens, calls to `ConsumeToken()` only reset the
// refill "timer" (pushing away the next refill to happen after
// `refill_interval` from now).
class CommitQuota {
 public:
  CommitQuota(int max_tokens, base::TimeDelta refill_interval);

  CommitQuota(const CommitQuota&);
  CommitQuota& operator=(const CommitQuota&);
  CommitQuota(CommitQuota&&);
  CommitQuota& operator=(CommitQuota&&);

  ~CommitQuota();

  // Changes the current quota params to the values provided as arguments.
  void SetParams(int max_tokens, base::TimeDelta refill_interval);

  // Returns whether the current token count is greater than zero.
  bool HasTokensAvailable();

  // Consumes a token. If the current token count is zero, it only resets the
  // refill "timer".
  void ConsumeToken();

 private:
  // Refills any tokens and updates `last_refilled_` time if possible.
  void RefillTokens();

  // Params of the quota.
  int max_tokens_;
  base::TimeDelta refill_interval_;

  // Current state of the quota.
  int tokens_;
  base::TimeTicks last_refilled_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_
