// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/resource_tracker.h"

#include <tuple>

namespace page_load_metrics {

ResourceTracker::ResourceTracker() = default;

ResourceTracker::~ResourceTracker() = default;

void ResourceTracker::UpdateResourceDataUse(
    int process_id,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  // Clear the map of previous updates to prevent completed resources from
  // remaining in the map. This is safe because the map is only expected
  // to contain updates for resources that have actively received new data.
  previous_resource_updates_.clear();
  for (auto const& resource : resources) {
    ProcessResourceUpdate(process_id, resource);
  }
}

void ResourceTracker::ProcessResourceUpdate(
    int process_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  // Do not store resources loaded from the memory cache as we can receive
  // duplicate loads of resources across documents.
  if (resource->cache_type == page_load_metrics::mojom::CacheType::kMemory)
    return;

  content::GlobalRequestID global_id(process_id, resource->request_id);
  auto it = unfinished_resources_.find(global_id);

  // This is the first update received for a resource.
  if (it == unfinished_resources_.end()) {
    if (!resource->is_complete) {
      unfinished_resources_.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(global_id),
                                    std::forward_as_tuple(resource->Clone()));
    }
    return;
  }

  // Update the map of previous resource data now that we have a new data update
  // for a tracked resource.
  previous_resource_updates_[global_id] = it->second->Clone();

  // Either update the unfinished resource data or remove it from the map if it
  // completed. Must clone resource so it will be accessible when the observer
  // is destroyed.
  if (resource->is_complete)
    unfinished_resources_.erase(it);
  else
    it->second = resource->Clone();
}

bool ResourceTracker::HasPreviousUpdateForResource(
    content::GlobalRequestID request_id) const {
  return previous_resource_updates_.count(request_id) != 0;
}

const page_load_metrics::mojom::ResourceDataUpdatePtr&
ResourceTracker::GetPreviousUpdateForResource(
    content::GlobalRequestID request_id) const {
  DCHECK(HasPreviousUpdateForResource(request_id));
  return previous_resource_updates_.find(request_id)->second;
}

}  // namespace page_load_metrics
