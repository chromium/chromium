// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_THROTTLER_DOMAIN_SESSION_THROTTLER_H_
#define COMPONENTS_MESSAGES_ANDROID_THROTTLER_DOMAIN_SESSION_THROTTLER_H_

#include "base/containers/lru_cache.h"
#include "url/origin.h"

namespace messages {
// A simple wrapper of LRUCache to store domains on which
// the messages should not be displayed.
// Session means the throttler is valid during the lifecycle of
// app and is reset every time app is destroyed.
class DomainSessionThrottler {
 public:
  explicit DomainSessionThrottler(int capacity);
  ~DomainSessionThrottler();
  bool ShouldShow(url::Origin url);
  void AddStrike(url::Origin url);

 private:
  base::LRUCacheSet<url::Origin> cache_;
};
}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_THROTTLER_DOMAIN_SESSION_THROTTLER_H_
