// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/transferable_resource_tracker.h"

#include <GLES2/gl2.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

TransferableResourceTracker::TransferableResourceTracker()
    : starting_id_(kVizReservedRangeStartId.GetUnsafeValue()),
      next_id_(kVizReservedRangeStartId.GetUnsafeValue()) {}

TransferableResourceTracker::~TransferableResourceTracker() = default;

TransferableResource TransferableResourceTracker::ImportResource(
    std::unique_ptr<SurfaceSavedFrame> saved_frame) {
  DCHECK(saved_frame);
  // Since we will be dereferencing this blindly, CHECK that the frame is indeed
  // valid.
  CHECK(saved_frame->IsValid());

  // TODO(vmpstr): For now take only the root result. The remainder of the
  // resources will be released here.
  SurfaceSavedFrame::OutputCopyResult output_copy =
      std::move(saved_frame->TakeResult()->root_result);

  TransferableResource resource;
  if (output_copy.is_software) {
    // TODO(vmpstr): This needs to be updated and tested in software. For
    // example, we don't currently have a release callback in software, although
    // tests do set one up.
    resource = TransferableResource::MakeSoftware(
        output_copy.mailbox, output_copy.rect.size(), RGBA_8888);
  } else {
    resource = TransferableResource::MakeGL(
        output_copy.mailbox, GL_LINEAR, GL_TEXTURE_2D, output_copy.sync_token,
        output_copy.rect.size(),
        /*is_overlay_candidate=*/false);
  }

  resource.id = GetNextAvailableResourceId();
  DCHECK(!base::Contains(managed_resources_, resource.id));
  managed_resources_.emplace(
      resource.id, TransferableResourceHolder(
                       resource, std::move(output_copy.release_callback)));
  return resource;
}

void TransferableResourceTracker::RefResource(ResourceId id) {
  DCHECK(base::Contains(managed_resources_, id));
  ++managed_resources_[id].ref_count;
}

void TransferableResourceTracker::UnrefResource(ResourceId id) {
  DCHECK(base::Contains(managed_resources_, id));
  if (--managed_resources_[id].ref_count == 0)
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
    TransferableResourceHolder(
        const TransferableResource& resource,
        std::unique_ptr<SingleReleaseCallback> release_callback)
    : resource(resource),
      release_callback(std::move(release_callback)),
      ref_count(1u) {}

TransferableResourceTracker::TransferableResourceHolder::
    ~TransferableResourceHolder() {
  if (release_callback)
    release_callback->Run(resource.mailbox_holder.sync_token,
                          /*is_lost=*/false);
}

TransferableResourceTracker::TransferableResourceHolder&
TransferableResourceTracker::TransferableResourceHolder::operator=(
    TransferableResourceHolder&& other) = default;

}  // namespace viz
