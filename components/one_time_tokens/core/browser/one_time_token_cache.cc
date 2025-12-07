// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_cache.h"

#include <algorithm>

#include "base/time/time.h"

namespace one_time_tokens {

OneTimeTokenCache::OneTimeTokenCache(base::TimeDelta max_age)
    : max_age_(max_age) {}

OneTimeTokenCache::~OneTimeTokenCache() = default;

bool OneTimeTokenCache::PurgeExpiredAndAdd(const OneTimeToken& token) {
  PurgeExpired();

  // Don't replace the token if it exists already.
  if (std::ranges::find(tokens_, token) != tokens_.end()) {
    return false;
  }

  // Insert the new token while maintaining the sort order (oldest to newest).
  auto it = std::ranges::lower_bound(tokens_, token.on_device_arrival_time(),
                                     {}, &OneTimeToken::on_device_arrival_time);
  tokens_.insert(it, token);
  return true;
}

const std::list<OneTimeToken>& OneTimeTokenCache::PurgeExpiredAndGetCache() {
  PurgeExpired();
  return tokens_;
}

const std::list<OneTimeToken>& OneTimeTokenCache::GetCache() const {
  return tokens_;
}

void OneTimeTokenCache::PurgeExpired() {
  base::Time now = base::Time::Now();
  tokens_.remove_if([&](const OneTimeToken& cached_token) {
    return now - cached_token.on_device_arrival_time() > max_age_;
  });
}

}  // namespace one_time_tokens
