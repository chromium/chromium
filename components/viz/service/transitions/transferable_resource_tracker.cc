// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/transferable_resource_tracker.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace viz {

TransferableResourceTracker::TransferableResourceTracker(
    ReservedResourceIdTracker* id_tracker)
    : id_tracker_(id_tracker) {
  CHECK(id_tracker_);
}

TransferableResourceTracker::~TransferableResourceTracker() = default;

TransferableResourceTracker::ResourceFrame
TransferableResourceTracker::ImportResources(
    SurfaceSavedFrame::FrameResult frame_result,
    CompositorFrameTransitionDirective directive) {
  ResourceFrame resource_frame;
  resource_frame.shared.resize(frame_result.shared_results.size());
  for (size_t i = 0; i < frame_result.shared_results.size(); ++i) {
    auto& shared_result = frame_result.shared_results[i];
    if (shared_result.has_value()) {
      resource_frame.shared[i].emplace(
          ImportResource(std::move(*shared_result)));
      auto view_transition_element_resource_id =
          directive.shared_elements()[i].view_transition_element_resource_id;
      if (view_transition_element_resource_id.IsValid()) {
        resource_frame
            .element_id_to_resource[view_transition_element_resource_id] =
            resource_frame.shared[i]->resource;
      }
    }
  }

  for (auto resource_id : frame_result.empty_resource_ids) {
    DCHECK(!resource_frame.element_id_to_resource.contains(resource_id));
    resource_frame.element_id_to_resource[resource_id] = TransferableResource();
  }

  return resource_frame;
}

TransferableResourceTracker::PositionedResource
TransferableResourceTracker::ImportResource(
    SurfaceSavedFrame::OutputCopyResult output_copy) {
  auto resource = TransferableResource::Make(
      output_copy.shared_image,
      TransferableResource::ResourceSource::kViewTransition,
      output_copy.sync_token);

  TransferableResourceHolder::ResourceReleaseCallback release_callback;
  if (output_copy.release_callback) {
    release_callback = base::BindOnce(
        [](ReleaseCallback callback, const TransferableResource& resource,
           const gpu::SyncToken& sync_token) {
          std::move(callback).Run(sync_token, /*is_lost=*/false);
        },
        std::move(output_copy.release_callback));
  }

  resource.id = id_tracker_->AllocId(/*initial_ref_count=*/1);
  DCHECK(!base::Contains(managed_resources_, resource.id));
  managed_resources_.emplace(
      resource.id,
      TransferableResourceHolder(resource, std::move(release_callback)));

  PositionedResource result;
  result.resource = resource;
  return result;
}

void TransferableResourceTracker::ReturnFrame(const ResourceFrame& frame) {
  for (const auto& shared : frame.shared) {
    if (shared.has_value()) {
      UnrefResource(shared->resource.id, /*count=*/1, gpu::SyncToken());
    }
  }
}

void TransferableResourceTracker::RefResource(ResourceId id) {
  if (!base::Contains(managed_resources_, id)) {
    return;
  }

  id_tracker_->RefId(id, /*count=*/1);
}

void TransferableResourceTracker::UnrefResource(
    ResourceId id,
    int count,
    const gpu::SyncToken& sync_token) {
  if (!base::Contains(managed_resources_, id)) {
    return;
  }

  // Always update the release sync token, even if we're still keeping the
  // resource. This way, if we first return it from the display compositor and
  // then release it from surface animation manager, we will still have the
  // right sync token.
  if (sync_token.HasData()) {
    auto it = managed_resources_.find(id);
    CHECK(it != managed_resources_.end());
    it->second.release_sync_token = sync_token;
  }

  if (id_tracker_->UnrefId(id, count)) {
    managed_resources_.erase(id);
  }
}

TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder() = default;
TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder(TransferableResourceHolder&& other) = default;
TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder(const TransferableResource& resource,
                               ResourceReleaseCallback release_callback)
    : resource(resource), release_callback(std::move(release_callback)) {}

TransferableResourceTracker::TransferableResourceHolder::
    ~TransferableResourceHolder() {
  if (release_callback) {
    std::move(release_callback).Run(resource, release_sync_token);
  }
}

TransferableResourceTracker::TransferableResourceHolder&
TransferableResourceTracker::TransferableResourceHolder::operator=(
    TransferableResourceHolder&& other) = default;

TransferableResourceTracker::ResourceFrame::ResourceFrame() = default;
TransferableResourceTracker::ResourceFrame::ResourceFrame(
    ResourceFrame&& other) = default;
TransferableResourceTracker::ResourceFrame::~ResourceFrame() = default;

TransferableResourceTracker::ResourceFrame&
TransferableResourceTracker::ResourceFrame::operator=(ResourceFrame&& other) =
    default;

}  // namespace viz
