// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/lite_video/lite_video_hint_agent.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/lite_video/lite_video_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace lite_video {

LiteVideoHintAgent::LiteVideoHintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<LiteVideoHintAgent>(render_frame) {
  DCHECK(render_frame);
}

LiteVideoHintAgent::~LiteVideoHintAgent() = default;

void LiteVideoHintAgent::OnDestruct() {
  delete this;
}

void LiteVideoHintAgent::AddThrottle(LiteVideoURLLoaderThrottle* throttle) {
  DCHECK(HasLiteVideoHint());
  active_throttles_.insert(throttle);
  UMA_HISTOGRAM_COUNTS("LiteVideo.HintAgent.ActiveThrottleSize",
                       active_throttles_.size());
}

void LiteVideoHintAgent::RemoveThrottle(LiteVideoURLLoaderThrottle* throttle) {
  active_throttles_.erase(throttle);
}

base::TimeDelta LiteVideoHintAgent::CalculateLatencyForResourceResponse(
    const network::mojom::URLResponseHead& response_head) {
  if (!HasLiteVideoHint())
    return base::TimeDelta();

  if (active_throttles_.size() >= GetMaxActiveThrottles())
    return base::TimeDelta();

  if (ShouldDisableLiteVideoForCacheControlNoTransform() &&
      response_head.headers &&
      response_head.headers->HasHeaderValue("cache-control", "no-transform")) {
    return base::TimeDelta();
  }

  int64_t recv_bytes = response_head.content_length;
  if (recv_bytes == -1)
    recv_bytes = response_head.encoded_body_length;
  if (recv_bytes == -1 && !ShouldThrottleLiteVideoMissingContentLength()) {
    return base::TimeDelta();
  } else if (recv_bytes == -1) {
    recv_bytes = 0;
  }

  if (kilobytes_buffered_before_throttle_ <
      *kilobytes_to_buffer_before_throttle_) {
    kilobytes_buffered_before_throttle_ += recv_bytes / 1024;
    return base::TimeDelta();
  }

  // The total RTT for this media response should be based on how much time it
  // took to transfer the packet in the target bandwidth, and the per RTT
  // latency. For example, assuming 100KBPS target bandwidth and target RTT of
  // 1 second, an 400KB response should have total delay of 5 seconds (400/100
  // + 1).
  auto delay_for_throttled_response =
      base::TimeDelta::FromSecondsD(
          recv_bytes / (*target_downlink_bandwidth_kbps_ * 1024.0)) +
      *target_downlink_rtt_latency_;
  auto response_delay =
      response_head.response_time - response_head.request_time;
  if (delay_for_throttled_response <= response_delay)
    return base::TimeDelta();

  return std::min(delay_for_throttled_response - response_delay,
                  *max_throttling_delay_);
}

bool LiteVideoHintAgent::HasLiteVideoHint() const {
  return target_downlink_bandwidth_kbps_ && target_downlink_rtt_latency_ &&
         kilobytes_to_buffer_before_throttle_ && max_throttling_delay_;
}

void LiteVideoHintAgent::SetLiteVideoHint(
    blink::mojom::LiteVideoHintPtr lite_video_hint) {
  if (!lite_video_hint)
    return;
  target_downlink_bandwidth_kbps_ =
      lite_video_hint->target_downlink_bandwidth_kbps;
  kilobytes_to_buffer_before_throttle_ =
      lite_video_hint->kilobytes_to_buffer_before_throttle;
  target_downlink_rtt_latency_ = lite_video_hint->target_downlink_rtt_latency;
  max_throttling_delay_ = lite_video_hint->max_throttling_delay;
  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.HintAgent.HasHint", true);
}

void LiteVideoHintAgent::StopThrottlingAndClearHints() {
  for (auto* throttle : active_throttles_)
    throttle->ResumeIfThrottled();
  kilobytes_buffered_before_throttle_ = 0;
  target_downlink_bandwidth_kbps_.reset();
  kilobytes_to_buffer_before_throttle_.reset();
  target_downlink_rtt_latency_.reset();
  max_throttling_delay_.reset();
}

}  // namespace lite_video
