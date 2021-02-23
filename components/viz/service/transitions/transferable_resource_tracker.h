// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_

#include <map>
#include <memory>

#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// This class is a simple transferable resource generator and lifetime tracker.
// Note that TransferableResourceTracker uses reserved range ResourceIds.
class VIZ_SERVICE_EXPORT TransferableResourceTracker {
 public:
  TransferableResourceTracker();
  TransferableResourceTracker(const TransferableResourceTracker&) = delete;
  ~TransferableResourceTracker();

  TransferableResourceTracker& operator=(const TransferableResourceTracker&) =
      delete;

  TransferableResource ImportResource(
      std::unique_ptr<SurfaceSavedFrame> saved_frame);

  void RefResource(ResourceId id);
  void UnrefResource(ResourceId id);

  bool is_empty() const { return managed_resources_.empty(); }

 private:
  friend class TransferableResourceTrackerTest;

  ResourceId GetNextAvailableResourceId();

  TransferableResource ImportTextureResult(
      SurfaceSavedFrame::TextureResult result);

  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "ResourceId underlying type should be uint32_t");

  const uint32_t starting_id_;
  uint32_t next_id_;

  struct TransferableResourceHolder {
    TransferableResourceHolder();
    TransferableResourceHolder(TransferableResourceHolder&& other);
    TransferableResourceHolder(
        const TransferableResource& resource,
        std::unique_ptr<SingleReleaseCallback> release_callback);
    ~TransferableResourceHolder();

    TransferableResourceHolder& operator=(TransferableResourceHolder&& other);

    TransferableResource resource;
    std::unique_ptr<SingleReleaseCallback> release_callback;
    uint8_t ref_count;
  };

  std::map<ResourceId, TransferableResourceHolder> managed_resources_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_
