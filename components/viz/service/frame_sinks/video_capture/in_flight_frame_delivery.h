// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"

namespace viz {

// Represents an in-flight frame delivery to the consumer. Its main purpose is
// to proxy callbacks from the consumer back to the relevant capturer
// components owned and operated by FrameSinkVideoCapturerImpl.
class VIZ_SERVICE_EXPORT InFlightFrameDelivery
    : public mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  InFlightFrameDelivery(base::OnceClosure post_delivery_callback,
                        base::OnceCallback<void(double)> feedback_callback);

  ~InFlightFrameDelivery() final;

  // mojom::FrameSinkVideoConsumerFrameCallbacks implementation:
  void Done() final;
  void ProvideFeedback(double utilization) final;

 private:
  base::OnceClosure post_delivery_callback_;
  base::OnceCallback<void(double)> feedback_callback_;

  DISALLOW_COPY_AND_ASSIGN(InFlightFrameDelivery);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_
