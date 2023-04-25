// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/transferable_resource_tracker.h"

#include <GLES2/gl2.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

TransferableResourceTracker::TransferableResourceTracker(
    SharedBitmapManager* shared_bitmap_manager)
    : starting_id_(kVizReservedRangeStartId.GetUnsafeValue()),
      next_id_(kVizReservedRangeStartId.GetUnsafeValue()),
      shared_bitmap_manager_(shared_bitmap_manager) {}

TransferableResourceTracker::~TransferableResourceTracker() = default;

TransferableResourceTracker::ResourceFrame
TransferableResourceTracker::ImportResources(
    std::unique_ptr<SurfaceSavedFrame> saved_frame) {
  DCHECK(saved_frame);
  // Since we will be dereferencing this blindly, CHECK that the frame is indeed
  // valid.
  CHECK(saved_frame->IsValid());

  absl::optional<SurfaceSavedFrame::FrameResult> frame_copy =
      saved_frame->TakeResult();
  const auto& directive = saved_frame->directive();

  ResourceFrame resource_frame;
  resource_frame.root = ImportResource(std::move(frame_copy->root_result));

  resource_frame.shared.resize(frame_copy->shared_results.size());
  for (size_t i = 0; i < frame_copy->shared_results.size(); ++i) {
    auto& shared_result = frame_copy->shared_results[i];
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

  for (auto resource_id : frame_copy->empty_resource_ids) {
    DCHECK(!resource_frame.element_id_to_resource.contains(resource_id));
    resource_frame.element_id_to_resource[resource_id] = TransferableResource();
  }

  return resource_frame;
}

TransferableResourceTracker::PositionedResource
TransferableResourceTracker::ImportResource(
    SurfaceSavedFrame::OutputCopyResult output_copy) {
  TransferableResource resource;

  TransferableResourceHolder::ResourceReleaseCallback release_callback;
  if (output_copy.is_software) {
    DCHECK(output_copy.mailbox.IsZero());
    DCHECK(!output_copy.release_callback);

    SharedBitmapId id = SharedBitmap::GenerateId();
    shared_bitmap_manager_->LocalAllocatedSharedBitmap(
        std::move(output_copy.bitmap), id);
    resource = TransferableResource::MakeSoftware(
        id, output_copy.draw_data.size, SinglePlaneFormat::kRGBA_8888);

    // Remove the bitmap from shared bitmap manager when no longer in use.
    release_callback = base::BindOnce(
        [](SharedBitmapManager* manager, const TransferableResource& resource) {
          const SharedBitmapId& id = resource.mailbox_holder.mailbox;
          manager->ChildDeletedSharedBitmap(id);
        },
        shared_bitmap_manager_);
  } else {
    DCHECK(output_copy.bitmap.drawsNothing());

    resource = TransferableResource::MakeGpu(
        output_copy.mailbox, GL_TEXTURE_2D, output_copy.sync_token,
        output_copy.draw_data.size, SinglePlaneFormat::kRGBA_8888,
        /*is_overlay_candidate=*/false);
    resource.color_space = output_copy.color_space;

    // Run the SingleReleaseCallback when no longer in use.
    if (output_copy.release_callback) {
      release_callback = base::BindOnce(
          [](ReleaseCallback callback, const TransferableResource& resource) {
            std::move(callback).Run(resource.mailbox_holder.sync_token,
                                    /*is_lost=*/false);
          },
          std::move(output_copy.release_callback));
    }
  }

  resource.id = GetNextAvailableResourceId();
  DCHECK(!base::Contains(managed_resources_, resource.id));
  managed_resources_.emplace(
      resource.id,
      TransferableResourceHolder(resource, std::move(release_callback)));

  PositionedResource result;
  result.resource = resource;
  result.draw_data = output_copy.draw_data;
  return result;
}

void TransferableResourceTracker::ReturnFrame(const ResourceFrame& frame) {
  UnrefResource(frame.root.resource.id, /*count=*/1);
  for (const auto& shared : frame.shared) {
    if (shared.has_value())
      UnrefResource(shared->resource.id, /*count=*/1);
  }
}

void TransferableResourceTracker::RefResource(ResourceId id) {
  DCHECK(base::Contains(managed_resources_, id));
  ++managed_resources_[id].ref_count;
}

void TransferableResourceTracker::UnrefResource(ResourceId id, int count) {
  DCHECK(base::Contains(managed_resources_, id));
  DCHECK_LE(count, managed_resources_[id].ref_count);
  managed_resources_[id].ref_count -= count;
  if (managed_resources_[id].ref_count == 0)
    managed_resources_.erase(id);
}

ResourceId TransferableResourceTracker::GetNextAvailableResourceId() {
  uint32_t result = next_id_;

  // Since we're working with a limit range of resources, it is a lot more
  // likely that we will loop back to the starting id after running out of
  // resource ids. This loop ensures that `next_id_` is set to a value that is
  // not `result` and is also available in that it's currently tracked by
  // `managed_resources_`. Note that if we end up looping twice, we fail with a
  // CHECK since we don't have any available resources for this request.
  bool looped = false;
  while (next_id_ == result ||
         base::Contains(managed_resources_, ResourceId(next_id_))) {
    if (next_id_ == std::numeric_limits<uint32_t>::max()) {
      CHECK(!looped);
      next_id_ = starting_id_;
      looped = true;
    } else {
      ++next_id_;
    }
  }
  DCHECK_GE(result, kVizReservedRangeStartId.GetUnsafeValue());
  return ResourceId(result);
}

TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder() = default;
TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder(TransferableResourceHolder&& other) = default;
TransferableResourceTracker::TransferableResourceHolder::
    TransferableResourceHolder(const TransferableResource& resource,
                               ResourceReleaseCallback release_callback)
    : resource(resource),
      release_callback(std::move(release_callback)),
      ref_count(1u) {}

TransferableResourceTracker::TransferableResourceHolder::
    ~TransferableResourceHolder() {
  if (release_callback)
    std::move(release_callback).Run(resource);
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
