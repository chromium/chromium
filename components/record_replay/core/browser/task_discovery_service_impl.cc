// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_discovery_service_impl.h"

#include "components/record_replay/core/browser/task_provider.h"
#include "url/gurl.h"

namespace record_replay {

TaskDiscoveryServiceImpl::TaskDiscoveryServiceImpl()
    : providers_(TaskProvider::CreateProviders()) {}

TaskDiscoveryServiceImpl::~TaskDiscoveryServiceImpl() = default;

void TaskDiscoveryServiceImpl::ShouldOfferTask(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  cached_metadata_.reset();
  QueryNextProvider(0, url, std::move(callback));
}

void TaskDiscoveryServiceImpl::QueryNextProvider(
    size_t index,
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  if (index >= providers_.size()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/504514117): What if the provider does not respond? Improve
  // this interface to allow for a timeout.
  providers_[index]->ShouldOfferTask(
      url,
      base::BindOnce(&TaskDiscoveryServiceImpl::OnProviderResponse,
                     base::Unretained(this), index, url, std::move(callback)));
}

void TaskDiscoveryServiceImpl::OnProviderResponse(
    size_t index,
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    std::optional<TaskDiscoveryService::AutomationMetadata> metadata) {
  if (metadata.has_value()) {
    cached_metadata_ = std::move(metadata);
    std::move(callback).Run(true);
    return;
  }

  QueryNextProvider(index + 1, url, std::move(callback));
}

std::optional<TaskDiscoveryService::AutomationMetadata>
TaskDiscoveryServiceImpl::GetMetadata() {
  return cached_metadata_;
}

}  // namespace record_replay
