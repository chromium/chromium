// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_CLIENT_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_CLIENT_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/pipeline_status.h"
#include "media/base/waiting.h"

namespace chromecast {
namespace media {

struct AvPipelineClient {
  typedef base::RepeatingCallback<
      void(base::TimeDelta, base::TimeDelta, base::TimeTicks)>
      TimeUpdateCB;

  AvPipelineClient();
  AvPipelineClient(AvPipelineClient&& other);
  AvPipelineClient(const AvPipelineClient& other) = delete;
  AvPipelineClient& operator=(AvPipelineClient&& other);
  AvPipelineClient& operator=(const AvPipelineClient& other) = delete;
  ~AvPipelineClient();

  // Waiting status notification.
  ::media::WaitingCB waiting_cb;

  // End of stream notification.
  base::RepeatingClosure eos_cb;

  // Asynchronous playback error notification.
  ::media::PipelineStatusCB playback_error_cb;

  // Callback used to report the playback statistics.
  ::media::StatisticsCB statistics_cb;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_CLIENT_H_
