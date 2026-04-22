// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_DISCOVERY_SERVICE_IMPL_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_DISCOVERY_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/record_replay/core/browser/activity_discovery_service.h"

namespace record_replay {

class ActivityProvider;

class ActivityDiscoveryServiceImpl : public ActivityDiscoveryService {
 public:
  ActivityDiscoveryServiceImpl();
  ~ActivityDiscoveryServiceImpl() override;

  // ActivityDiscoveryService:
  void ShouldOfferActivity(const GURL& url,
                           base::OnceCallback<void(bool)> callback) override;
  std::optional<ActivityDiscoveryService::AutomationMetadata> GetMetadata()
      override;

 private:
  // Providers are queried in order, and the first one that offers an activity
  // is used.
  void QueryNextProvider(size_t index,
                         const GURL& url,
                         base::OnceCallback<void(bool)> callback);
  void OnProviderResponse(
      size_t index,
      const GURL& url,
      base::OnceCallback<void(bool)> callback,
      std::optional<ActivityDiscoveryService::AutomationMetadata> metadata);

  std::vector<std::unique_ptr<ActivityProvider>> providers_;
  std::optional<ActivityDiscoveryService::AutomationMetadata> cached_metadata_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_DISCOVERY_SERVICE_IMPL_H_
