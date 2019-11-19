// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace chromecast {

class CastWindowManagerAura;

class CastExternalBeginFrameSource {
 public:
  explicit CastExternalBeginFrameSource(
      CastWindowManagerAura* cast_window_manager_aura);
  ~CastExternalBeginFrameSource();

 private:
  void OnFrameComplete(const viz::BeginFrameAck& ack);
  void IssueExternalBeginFrame();

  CastWindowManagerAura* cast_window_manager_aura_;
  uint64_t sequence_number_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<CastExternalBeginFrameSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastExternalBeginFrameSource);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_SOURCE_H_
