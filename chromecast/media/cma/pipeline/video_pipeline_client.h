// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_CLIENT_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_CLIENT_H_

#include "base/functional/callback.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"

namespace gfx {
class Size;
}

namespace chromecast {
namespace media {

struct VideoPipelineClient {
  typedef base::RepeatingCallback<void(const gfx::Size& natural_size)>
      NaturalSizeChangedCB;

  VideoPipelineClient();
  VideoPipelineClient(VideoPipelineClient&& other);
  VideoPipelineClient(const VideoPipelineClient& other) = delete;
  VideoPipelineClient& operator=(VideoPipelineClient&& other);
  VideoPipelineClient& operator=(const VideoPipelineClient& other) = delete;
  ~VideoPipelineClient();

  // All the default callbacks.
  AvPipelineClient av_pipeline_client;

  // Video resolution change notification.
  NaturalSizeChangedCB natural_size_changed_cb;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_CLIENT_H_
