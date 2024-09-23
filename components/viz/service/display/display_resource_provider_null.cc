// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_null.h"

#include <utility>
#include <vector>

#include "base/not_fatal_until.h"

namespace viz {

DisplayResourceProviderNull::DisplayResourceProviderNull()
    : DisplayResourceProvider(DisplayResourceProvider::kGpu) {}

DisplayResourceProviderNull::~DisplayResourceProviderNull() {
  Destroy();
}

std::vector<ReturnedResource>
DisplayResourceProviderNull::DeleteAndReturnUnusedResourcesToChildImpl(
    Child& child_info,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  std::vector<ReturnedResource> to_return;
  to_return.reserve(unused.size());

  for (ResourceId local_id : unused) {
    auto it = resources_.find(local_id);
    CHECK(it != resources_.end(), base::NotFatalUntil::M130);
    ChildResource& resource = it->second;

    ResourceId child_id = resource.transferable.id;
    DCHECK(child_info.child_to_parent_map.count(child_id));

    auto can_delete = CanDeleteNow(child_info, resource, style);
    DCHECK_EQ(can_delete, CanDeleteNowResult::kYes);

    to_return.emplace_back(child_id, resource.sync_token(),
                           std::move(resource.release_fence),
                           resource.imported_count, /*is_lost=*/false);

    child_info.child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    resources_.erase(it);
  }

  return to_return;
}

}  // namespace viz
