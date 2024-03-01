// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/surface_resource_holder.h"

#include <utility>

#include "base/check.h"
#include "components/viz/service/frame_sinks/surface_resource_holder_client.h"

namespace viz {

ReservedResourceDelegate::~ReservedResourceDelegate() = default;

SurfaceResourceHolder::SurfaceResourceHolder(
    SurfaceResourceHolderClient* client)
    : client_(client) {}

SurfaceResourceHolder::~SurfaceResourceHolder() = default;

void SurfaceResourceHolder::Reset() {
  resource_id_info_map_.clear();
}

void SurfaceResourceHolder::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    // We don't handle reserved resources here. CompositorFrames from clients
    // can never contain reserved ResourceIds, but viz can pretend to submit
    // frames from the client (see
    // `CompositorFrameSinkSupport::SubmitCompositorFrameLocally`), e.g. for
    // frame eviction purposes. Those CompositorFrames may contain reserved
    // resources, so ignore them here.
    if (resource.id >= kVizReservedRangeStartId) {
      continue;
    }

    ResourceRefs& ref = resource_id_info_map_[resource.id];
    ref.refs_holding_resource_alive++;
    ref.refs_received_from_child++;
  }
}

void SurfaceResourceHolder::RefResources(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    // We don't handle reserved resources here.
    if (resource.id >= kVizReservedRangeStartId)
      continue;

    auto count_it = resource_id_info_map_.find(resource.id);
    DCHECK(count_it != resource_id_info_map_.end())
        << "ResourceId: " << resource.id;
    count_it->second.refs_holding_resource_alive++;
  }
}

void SurfaceResourceHolder::UnrefResources(
    std::vector<ReturnedResource> resources) {
  std::vector<ReturnedResource> resources_available_to_return;

  for (auto& resource : resources) {
    // We don't handle reserved resources here.
    if (resource.id >= kVizReservedRangeStartId)
      continue;

    auto count_it = resource_id_info_map_.find(resource.id);
    if (count_it == resource_id_info_map_.end())
      continue;
    ResourceRefs& ref = count_it->second;
    ref.refs_holding_resource_alive -= resource.count;
    // Keep the newest return sync token that has data.
    // TODO(jbauman): Handle the case with two valid sync tokens.
    if (resource.sync_token.HasData())
      ref.sync_token = resource.sync_token;
    if (ref.refs_holding_resource_alive == 0) {
      resource.sync_token = ref.sync_token;
      resource.count = ref.refs_received_from_child;
      resources_available_to_return.push_back(std::move(resource));
      resource_id_info_map_.erase(count_it);
    }
  }

  client_->ReturnResources(std::move(resources_available_to_return));
}

}  // namespace viz
