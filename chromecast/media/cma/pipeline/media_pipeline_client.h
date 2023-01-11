// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_CLIENT_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_CLIENT_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"

namespace chromecast {
namespace media {

struct MediaPipelineClient {
  typedef base::RepeatingCallback<
      void(base::TimeDelta, base::TimeDelta, base::TimeTicks)>
      TimeUpdateCB;

  MediaPipelineClient();
  MediaPipelineClient(MediaPipelineClient&& other);
  MediaPipelineClient(const MediaPipelineClient& other) = delete;
  MediaPipelineClient& operator=(MediaPipelineClient&& other);
  MediaPipelineClient& operator=(const MediaPipelineClient& other) = delete;
  ~MediaPipelineClient();

  // Callback used to report a playback error as a ::media::PipelineStatus.
  ::media::PipelineStatusCallback error_cb;

  // Callback used to report the latest playback time,
  // as well as the maximum time available for rendering.
  TimeUpdateCB time_update_cb;

  // Callback used to report the buffering status.
  ::media::BufferingStateCB buffering_state_cb;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_CLIENT_H_
