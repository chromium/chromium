// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_LEGACY_TOPICS_CLEANER_H_
#define COMPONENTS_INVALIDATION_LEGACY_TOPICS_CLEANER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace invalidation {

class IdentityProvider;
class PerUserTopicSubscriptionManager;

// Cleanup class to unsubscribe from unused legacy invalidation topics stored
// in local state.
// TODO(crbug.com/434619290): Keep it running for a year to ensure cleanup was
// triggered. Added in Aug 2025.
class LegacyTopicsCleaner {
 public:
  LegacyTopicsCleaner(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<IdentityProvider> identity_provider,
      PrefService* pref_service);
  ~LegacyTopicsCleaner();

  LegacyTopicsCleaner(const LegacyTopicsCleaner&) = delete;
  LegacyTopicsCleaner& operator=(const LegacyTopicsCleaner&) = delete;

 private:
  std::unique_ptr<IdentityProvider> identity_provider_;
  std::vector<std::unique_ptr<PerUserTopicSubscriptionManager>>
      topic_subscription_managers_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_LEGACY_TOPICS_CLEANER_H_
