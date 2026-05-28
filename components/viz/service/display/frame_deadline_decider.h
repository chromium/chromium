// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class VIZ_SERVICE_EXPORT FrameDeadlineDecider {
 public:
  FrameDeadlineDecider();
  ~FrameDeadlineDecider();

  FrameDeadlineDecider(const FrameDeadlineDecider&) = delete;
  FrameDeadlineDecider& operator=(const FrameDeadlineDecider&) = delete;

  // Returns the selected deadline. Stub always returns preferred.
  PossibleDeadline SelectDeadline(const PossibleDeadlines& possible_deadlines);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_DEADLINE_DECIDER_H_
