// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_CACHE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_CACHE_H_

#include <list>

#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace one_time_tokens {

// Cache for OneTimeTokens with a fixed expiration time.
class OneTimeTokenCache {
 public:
  explicit OneTimeTokenCache(base::TimeDelta max_age);
  OneTimeTokenCache(const OneTimeTokenCache&) = delete;
  OneTimeTokenCache& operator=(const OneTimeTokenCache&) = delete;
  ~OneTimeTokenCache();

  // Purges expired tokens and adds `token` to the cache if it is new.
  // Returns true if a token with the same value did not exist in the cache
  // (ignoring on_device_arrival_time_ in the comparison).
  bool PurgeExpiredAndAdd(const OneTimeToken& token);

  // Purges expired tokens and returns the remaining tokens.
  const std::list<OneTimeToken>& PurgeExpiredAndGetCache();

  // Returns all the tokens without filtering for expiration.
  const std::list<OneTimeToken>& GetCache() const;

 private:
  void PurgeExpired();

  // Tokens sorted from oldest to newest.
  std::list<OneTimeToken> tokens_;
  base::TimeDelta max_age_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_CACHE_H_
