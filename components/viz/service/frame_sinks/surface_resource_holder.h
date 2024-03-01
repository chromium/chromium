// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_

#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class SurfaceResourceHolderClient;

// ReservedResourceDelegate is an interface for tracking the lifetime of
// resources. Code which submits resources that are managed fully on the viz
// side (with resource IDs >= kVizReservedRangeStartId) should implement this.
class ReservedResourceDelegate {
 public:
  virtual ~ReservedResourceDelegate();

  virtual void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) = 0;
  virtual void RefResources(
      const std::vector<TransferableResource>& resources) = 0;
  virtual void UnrefResources(
      const std::vector<ReturnedResource>& resources) = 0;
};

// A SurfaceResourceHolder manages the lifetime of resources submitted by a
// particular SurfaceFactory. Each resource is held by the service until
// it is no longer referenced by any pending frames or by any
// resource providers.
class VIZ_SERVICE_EXPORT SurfaceResourceHolder {
 public:
  explicit SurfaceResourceHolder(SurfaceResourceHolderClient* client);

  SurfaceResourceHolder(const SurfaceResourceHolder&) = delete;
  SurfaceResourceHolder& operator=(const SurfaceResourceHolder&) = delete;

  ~SurfaceResourceHolder();

  void Reset();
  void ReceiveFromChild(const std::vector<TransferableResource>& resources);
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(std::vector<ReturnedResource> resources);

 private:
  raw_ptr<SurfaceResourceHolderClient> client_;

  struct ResourceRefs {
    int refs_received_from_child = 0;
    int refs_holding_resource_alive = 0;
    gpu::SyncToken sync_token;
  };
  // Keeps track of the number of users currently in flight for each resource
  // ID we've received from the client. When this counter hits zero for a
  // particular resource, that ID is available to return to the client with
  // the most recently given non-empty sync token.
  using ResourceIdInfoMap =
      std::unordered_map<ResourceId, ResourceRefs, ResourceIdHasher>;
  ResourceIdInfoMap resource_id_info_map_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_
