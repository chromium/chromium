// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/pending_copy_output_request.h"

#include <utility>

namespace viz {

PendingCopyOutputRequest::PendingCopyOutputRequest(
    LocalSurfaceId surface_id,
    SubtreeCaptureId subtree_id,
    std::unique_ptr<CopyOutputRequest> request,
    bool capture_exact_id)
    : local_surface_id(surface_id),
      subtree_capture_id(subtree_id),
      copy_output_request(std::move(request)),
      capture_exact_surface_id(capture_exact_id) {}

PendingCopyOutputRequest::PendingCopyOutputRequest(PendingCopyOutputRequest&&) =
    default;

PendingCopyOutputRequest& PendingCopyOutputRequest::operator=(
    PendingCopyOutputRequest&&) = default;

PendingCopyOutputRequest::~PendingCopyOutputRequest() = default;

}  // namespace viz
