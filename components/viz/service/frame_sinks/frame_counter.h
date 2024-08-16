// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_COUNTER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_COUNTER_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/mojom/compositing/frame_sinks_metrics_recorder.mojom-forward.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-shared.h"

namespace viz {

// Counts presented frames per frame sink. Used in tests to provide fps data
// per frame sink.
class VIZ_SERVICE_EXPORT FrameCounter {
 public:
  FrameCounter(base::TimeTicks start_time, base::TimeDelta bucket_size);
  ~FrameCounter();

  // Add a record for a frame sink.
  void AddFrameSink(const FrameSinkId& frame_sink_id,
                    mojom::CompositorFrameSinkType type,
                    bool is_root,
                    std::string_view debug_label);

  // Add a presented frame for the frame sink.
  void AddPresentedFrame(const FrameSinkId& frame_sink_id,
                         base::TimeTicks present_timetamp);

  // Takes the collected frame counts.
  mojom::FrameCountingDataPtr TakeData();

  // Sets a frame sink's type.
  void SetFrameSinkType(const FrameSinkId& frame_sink_id,
                        mojom::CompositorFrameSinkType type);

  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              std::string_view debug_label);

 private:
  // Time when the frame counting is stated.
  const base::TimeTicks start_time_;

  // Bucket size of the frame count records.
  const base::TimeDelta bucket_size_;

  base::flat_map<FrameSinkId, mojom::FrameCountingPerSinkDataPtr>
      frame_sink_data_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_COUNTER_H_
