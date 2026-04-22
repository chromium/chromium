// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/activity_discovery_service_impl.h"

#include "components/record_replay/core/browser/activity_provider.h"
#include "url/gurl.h"

namespace record_replay {

ActivityDiscoveryServiceImpl::ActivityDiscoveryServiceImpl()
    : providers_(ActivityProvider::CreateProviders()) {}

ActivityDiscoveryServiceImpl::~ActivityDiscoveryServiceImpl() = default;

void ActivityDiscoveryServiceImpl::ShouldOfferActivity(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  cached_metadata_.reset();
  QueryNextProvider(0, url, std::move(callback));
}

void ActivityDiscoveryServiceImpl::QueryNextProvider(
    size_t index,
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  if (index >= providers_.size()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/504514117): What if the provider does not respond? Improve
  // this interface to allow for a timeout.
  providers_[index]->ShouldOfferActivity(
      url,
      base::BindOnce(&ActivityDiscoveryServiceImpl::OnProviderResponse,
                     base::Unretained(this), index, url, std::move(callback)));
}

void ActivityDiscoveryServiceImpl::OnProviderResponse(
    size_t index,
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    std::optional<ActivityDiscoveryService::AutomationMetadata> metadata) {
  if (metadata.has_value()) {
    cached_metadata_ = std::move(metadata);
    std::move(callback).Run(true);
    return;
  }

  QueryNextProvider(index + 1, url, std::move(callback));
}

std::optional<ActivityDiscoveryService::AutomationMetadata>
ActivityDiscoveryServiceImpl::GetMetadata() {
  return cached_metadata_;
}

}  // namespace record_replay
