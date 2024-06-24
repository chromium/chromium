// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_IOS_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_IOS_H_

#include <memory>

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from CADisplayLink.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceIOS
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  explicit ExternalBeginFrameSourceIOS(uint32_t restart_id);

  ExternalBeginFrameSourceIOS(const ExternalBeginFrameSourceIOS&) = delete;
  ExternalBeginFrameSourceIOS& operator=(const ExternalBeginFrameSourceIOS&) =
      delete;

  ~ExternalBeginFrameSourceIOS() override;

  // ExternalBeginFrameSource override:
  void SetPreferredInterval(base::TimeDelta interval) override;
  base::TimeDelta GetMaximumRefreshFrameInterval() override;

  // ExternalBeginFrameSourceClient override.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // Called by CADisplayLinkImpl whenever there is a VSync update.
  void OnVSync(base::TimeTicks vsync_time,
               base::TimeTicks next_vsync_time,
               base::TimeDelta vsync_interval);

 private:
  // Toggles VSync updates.
  void SetEnabled(bool enabled);

  BeginFrameArgsGenerator begin_frame_args_generator_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_IOS_H_
