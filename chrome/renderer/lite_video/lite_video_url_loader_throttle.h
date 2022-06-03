// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_URL_LOADER_THROTTLE_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
class WebURLRequest;
}  // namespace blink

namespace lite_video {

// This class throttles media url requests that have audio/video mime-type in
// response headers, to simulate low bandwidth conditions. This allows MSE video
// players to adapt and play the lower resolution videos.
class LiteVideoURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit LiteVideoURLLoaderThrottle(int render_frame_id);
  ~LiteVideoURLLoaderThrottle() override;

  // Creates throttle for |request| if LiteVideo is enabled for LiteMode users,
  // and LiteVideoHintAgent has received hints for the navigation. Throttle will
  // be created only for Fetch/XHR requests.
  static std::unique_ptr<LiteVideoURLLoaderThrottle> MaybeCreateThrottle(
      const blink::WebURLRequest& request,
      int render_frame_id);

  // Resumes the media response if it was currently throttled. Otherwise its a
  // no-op.
  void ResumeIfThrottled();

  // blink::URLLoaderThrottle:
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  void DetachFromCurrentSequence() override;

 private:
  // Resumes the media response immediately.
  void ResumeThrottledMediaResponse();

  // Render frame id to get the media throttle observer of the render frame.
  const int render_frame_id_;

  // Timer to introduce latency for the response.
  std::unique_ptr<base::OneShotTimer> response_delay_timer_;
};

}  // namespace lite_video

#endif  // CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_URL_LOADER_THROTTLE_H_
