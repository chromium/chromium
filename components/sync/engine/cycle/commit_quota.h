// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_

#include "base/time/time.h"

namespace syncer {

// Tracks quota for commits with `initial_tokens` being also the maximum token
// count. A token is refilled every `refill_interval` if below the maximum token
// count. Tokens are consumed one by one by `ConsumeToken()` until they reach
// the zero. When having zero tokens, calls to `ConsumeToken()` only reset the
// refill "timer" (pushing away the next refill to happen after
// `refill_interval` from now).
class CommitQuota {
 public:
  CommitQuota(int initial_tokens, base::TimeDelta refill_interval);

  CommitQuota(const CommitQuota&) = delete;
  CommitQuota& operator=(const CommitQuota&) = delete;

  ~CommitQuota();

  // Returns whether the current token count is greater than zero.
  bool HasTokensAvailable();

  // Consumes a token. If the current token count is zero, it only resets the
  // refill "timer".
  void ConsumeToken();

 private:
  // Refills any tokens and updates `last_refilled_` time if possible.
  void RefillTokens();

  const int max_tokens_;
  const base::TimeDelta refill_interval_;

  int tokens_;
  base::TimeTicks last_refilled_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_COMMIT_QUOTA_H_
