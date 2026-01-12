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
    bool capture_exact_id,
    base::TimeDelta timeout)
    : local_surface_id(surface_id),
      subtree_capture_id(subtree_id),
      copy_output_request(std::move(request)),
      capture_exact_surface_id(capture_exact_id) {
  if (!timeout.is_zero()) {
    response_deadline_timer.emplace();
    response_deadline_timer->Start(
        FROM_HERE, timeout,
        base::BindOnce(&PendingCopyOutputRequest::TimeoutFired,
                       base::Unretained(this)));
  }
}

PendingCopyOutputRequest::~PendingCopyOutputRequest() = default;

bool PendingCopyOutputRequest::IsTimedOut() const {
  return !copy_output_request;
}

void PendingCopyOutputRequest::TimeoutFired() {
  if (!copy_output_request) {
    return;
  }
  copy_output_request->SendError(CopyOutputResult::Error::kTimeout);
  copy_output_request.reset();
}

}  // namespace viz
