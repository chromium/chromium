// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/throttler/domain_session_throttler.h"

namespace messages {

DomainSessionThrottler::DomainSessionThrottler(int capacity)
    : cache_(base::LRUCacheSet<url::Origin>(capacity)) {}

DomainSessionThrottler::~DomainSessionThrottler() {
  cache_.Clear();
}

bool DomainSessionThrottler::ShouldShow(url::Origin url) {
  if (cache_.max_size() == 0)
    return true;
  return cache_.Get(url) == cache_.end();
}

void DomainSessionThrottler::AddStrike(url::Origin url) {
  if (cache_.max_size() == 0)
    return;
  cache_.Put(url::Origin(url));
}

}  // namespace messages
