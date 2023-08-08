// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_PENDING_COPY_OUTPUT_REQUEST_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_PENDING_COPY_OUTPUT_REQUEST_H_

#include <memory>

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Encapsulates the necessary parameters to request a copy-of-output on a
// surface.
struct VIZ_SERVICE_EXPORT PendingCopyOutputRequest {
  PendingCopyOutputRequest(LocalSurfaceId surface_id,
                           SubtreeCaptureId subtree_id,
                           std::unique_ptr<CopyOutputRequest> request,
                           bool capture_exact_id = false);
  PendingCopyOutputRequest(PendingCopyOutputRequest&&);
  PendingCopyOutputRequest& operator=(PendingCopyOutputRequest&&);
  ~PendingCopyOutputRequest();

  // The ID of the local surface which |copy_output_request| will be placed on
  // its next composited frame. If this ID is default constructed, then the next
  // surface will provide the copy-of-output regardless of its LocalSurfaceId.
  LocalSurfaceId local_surface_id;

  // If valid, the |copy_output_request| will be placed on a render pass
  // associated with a layer subtree identified by this ID. Otherwise, it will
  // be placed on the root render pass.
  SubtreeCaptureId subtree_capture_id;

  // The actual copy-of-output request.
  std::unique_ptr<CopyOutputRequest> copy_output_request;

  // Indicates if this request is used to capture the old page for a navigation.
  // Such requests should only be picked up by the `Surfaces` with exact
  // matching `local_surface_id` (as opposed to being picked up by the surfaces
  // whose `LocalSurfaceId` is >= `local_surface_id`).
  //
  // Notice that setting this flag to true does not guarantee the successful
  // copy. The request issuer is also responsible for making sure the `Surface`s
  // are preserved in order to be captured.
  bool capture_exact_surface_id = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_PENDING_COPY_OUTPUT_REQUEST_H_
