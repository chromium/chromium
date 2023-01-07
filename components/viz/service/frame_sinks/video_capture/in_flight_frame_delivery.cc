// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/in_flight_frame_delivery.h"

#include <utility>

namespace viz {

InFlightFrameDelivery::InFlightFrameDelivery(
    base::OnceClosure post_delivery_callback,
    base::OnceCallback<void(const media::VideoCaptureFeedback&)>
        feedback_callback)
    : post_delivery_callback_(std::move(post_delivery_callback)),
      feedback_callback_(std::move(feedback_callback)) {}

InFlightFrameDelivery::~InFlightFrameDelivery() {
  Done();
}

void InFlightFrameDelivery::Done() {
  if (!post_delivery_callback_.is_null()) {
    std::move(post_delivery_callback_).Run();
  }
}

void InFlightFrameDelivery::ProvideFeedback(
    const media::VideoCaptureFeedback& feedback) {
  if (!feedback_callback_.is_null()) {
    std::move(feedback_callback_).Run(feedback);
  }
}

}  // namespace viz
