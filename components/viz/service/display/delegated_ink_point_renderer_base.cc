// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/delegated_ink_metadata.h"

namespace viz {

DelegatedInkPointRendererBase::DelegatedInkPointRendererBase() = default;
DelegatedInkPointRendererBase::~DelegatedInkPointRendererBase() = default;

void DelegatedInkPointRendererBase::InitMessagePipeline(
    mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver) {
  // The remote end of this pipeline exists on a per-tab basis, so if tab A
  // is using the feature and then tab B starts trying to use it, a new
  // PendingReceiver will arrive here while |receiver_| is still bound to the
  // remote in tab A. In this case, just reset |receiver_| so that tab A's
  // remote is unbound and bind the new receiver to use the feature in tab B.
  if (receiver_.is_bound()) {
    receiver_.reset();
    metadata_.reset();
    points_.clear();
  }
  receiver_.Bind(std::move(receiver));
}

void DelegatedInkPointRendererBase::FilterPoints() {
  if (points_.size() == 0)
    return;

  // |first_valid_point| is the first point in |points_| that will be drawn.
  // It will match |metadata_|'s timestamp and point because the app rendered
  // ink stroke and the delegated ink trail must overlap on the final point of
  // the ink stroke in order to connect seamlessly.
  auto first_valid_point = points_.find(metadata_->timestamp());

  // It is possible that this results in |points_| being empty. This occurs when
  // the points being forwarded from the browser process lose the race against
  // the ink metadata arriving in Display, including the point that matches the
  // metadata. There may still be old points in |points_| allowing execution to
  // get here, but none of them match the metadata point, so they are all
  // erased.
  points_.erase(points_.begin(), first_valid_point);

  // TODO(1052145): Add additional filtering to prevent points in |points_| from
  // having a timestamp that is far ahead of |metadata_|'s timestamp. This could
  // occur if the renderer stalls before sending a metadata while the browser
  // continues to pump points through to viz. Then when the renderer starts back
  // up again, the metadata it sends may be significantly older than the points
  // stored here, resulting in a long possibly incorrect trail if the max
  // number of points to store was reached.

  TRACE_EVENT_INSTANT1("viz", "Filtered points for delegated ink trail",
                       TRACE_EVENT_SCOPE_THREAD, "points", points_.size());
}

void DelegatedInkPointRendererBase::StoreDelegatedInkPoint(
    const DelegatedInkPoint& point) {
  TRACE_EVENT_INSTANT1("viz",
                       "DelegatedInkPointRendererImpl::StoreDelegatedInkPoint",
                       TRACE_EVENT_SCOPE_THREAD, "point", point.ToString());

  // Fail-safe to prevent storing excessive points if they are being sent but
  // never filtered and used, like if the renderer has stalled during a long
  // running script.
  if (points_.size() == kMaximumDelegatedInkPointsStored)
    points_.erase(points_.begin());

  points_.insert({point.timestamp(), point.point()});
}

}  // namespace viz
