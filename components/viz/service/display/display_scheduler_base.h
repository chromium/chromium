// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_BASE_H_

#include "base/time/time.h"
#include "components/viz/service/display/display_damage_tracker.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

struct BeginFrameAck;
class DisplayDamageTracker;

class VIZ_SERVICE_EXPORT DisplaySchedulerClient {
 public:
  virtual ~DisplaySchedulerClient() = default;

  // |expected_display_time| is used as video timestamps for capturing frame
  // sinks. DisplayScheduler passes the end of current VSync interval.
  virtual bool DrawAndSwap(base::TimeTicks expected_display_time) = 0;
  virtual void DidFinishFrame(const BeginFrameAck& ack) = 0;
};

class VIZ_SERVICE_EXPORT DisplaySchedulerBase
    : public DisplayDamageTracker::Observer {
 public:
  DisplaySchedulerBase();
  ~DisplaySchedulerBase() override;

  void SetClient(DisplaySchedulerClient* client);
  void SetDamageTracker(DisplayDamageTracker* damage_tracker);

  virtual void SetVisible(bool visible) = 0;
  virtual void ForceImmediateSwapIfPossible() = 0;
  virtual void SetNeedsOneBeginFrame(bool needs_draw) = 0;
  virtual void DidSwapBuffers() = 0;
  virtual void DidReceiveSwapBuffersAck() = 0;
  virtual void OutputSurfaceLost() = 0;

 protected:
  DisplaySchedulerClient* client_ = nullptr;
  DisplayDamageTracker* damage_tracker_ = nullptr;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_BASE_H_
