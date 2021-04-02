// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_
#define CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/common/lite_video_service.mojom.h"
#include "chrome/common/previews_resource_loading_hints.mojom.h"
#include "chrome/renderer/lite_video/lite_video_url_loader_throttle.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace lite_video {

// The renderer-side agent for LiteVideos. There is one instance per frame (main
// frame and subframes), to receive LiteVideo throttling parameters from
// browser.
class LiteVideoHintAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<LiteVideoHintAgent> {
 public:
  explicit LiteVideoHintAgent(content::RenderFrame* render_frame);
  ~LiteVideoHintAgent() override;

  LiteVideoHintAgent(const LiteVideoHintAgent&) = delete;
  LiteVideoHintAgent& operator=(const LiteVideoHintAgent&) = delete;

  // Returns how much time the media response should get throttled. This is the
  // difference between the target latency based on target bandwidth, RTT, and
  // the latency the response has already spent. Empty duration is returned when
  // the response should not be throttled. The first
  // |kilobytes_buffered_before_throttle_| for this render frame should not be
  // throttled. This function also updates
  // |kilobytes_buffered_before_throttle_|.
  base::TimeDelta CalculateLatencyForResourceResponse(
      const network::mojom::URLResponseHead& response_head);

  // Updates the LiteVideo throttling parameters for calculating
  // the latency to add to media requests.
  void SetLiteVideoHint(previews::mojom::LiteVideoHintPtr lite_video_hint);

  // Returns whether |this| has been provided a LiteVideoHint and
  // has the parameters needed for calculating the throttling latency.
  bool HasLiteVideoHint() const;

  void AddThrottle(LiteVideoURLLoaderThrottle* throttle);
  void RemoveThrottle(LiteVideoURLLoaderThrottle* throttle);

  const std::set<LiteVideoURLLoaderThrottle*>& GetActiveThrottlesForTesting()
      const {
    return active_throttles_;
  }

  // Stop throttling permanently. Resumes the current throttled media requests
  // immediately, and clears the hints so that throttling does not happen for
  // new requests.
  void StopThrottlingAndClearHints();

  // Notifies the response bytes that were throttled by LiteVideo.
  void NotifyThrottledDataUse(uint64_t response_bytes);

 private:
  friend class LiteVideoHintAgentTest;

  // content::RenderFrameObserver overrides
  void OnDestruct() override;

  // The network downlink bandwidth target in kilobytes per second used to
  // calculate the throttling delay on media requests
  base::Optional<int> target_downlink_bandwidth_kbps_;

  // The network downlink rtt target latency used to calculate the
  // throttling delay on media requests
  base::Optional<base::TimeDelta> target_downlink_rtt_latency_;

  // The number of kilobytes for media to be observed before starting to
  // throttle requests.
  base::Optional<int> kilobytes_to_buffer_before_throttle_;

  // The maximum delay a throttle can introduce for a media request in
  // milliseconds.
  base::Optional<base::TimeDelta> max_throttling_delay_;

  // The number of media KB that have been left unthrottled before starting
  // to introduce a throttling delay.
  int kilobytes_buffered_before_throttle_ = 0;

  // Set of media requests that are throttled currently. These are maintained
  // here to resume them immediately upon StopThrottling()
  std::set<LiteVideoURLLoaderThrottle*> active_throttles_;

  mojo::AssociatedRemote<mojom::LiteVideoService> lite_video_service_remote_;
};

}  // namespace lite_video

#endif  // CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_
