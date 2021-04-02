// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/lite_video/lite_video_url_loader_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/lite_video/lite_video_hint_agent.h"
#include "chrome/renderer/lite_video/lite_video_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace lite_video {

LiteVideoHintAgent* GetLiteVideoHintAgent(int render_frame_id) {
  DCHECK_NE(MSG_ROUTING_NONE, render_frame_id);
  if (auto* render_frame =
          content::RenderFrame::FromRoutingID(render_frame_id)) {
    return LiteVideoHintAgent::Get(render_frame);
  }
  return nullptr;
}

// static
std::unique_ptr<LiteVideoURLLoaderThrottle>
LiteVideoURLLoaderThrottle::MaybeCreateThrottle(
    const blink::WebURLRequest& request,
    int render_frame_id) {
  auto request_context = request.GetRequestContext();
  if (request_context != blink::mojom::RequestContextType::FETCH &&
      request_context != blink::mojom::RequestContextType::XML_HTTP_REQUEST) {
    return nullptr;
  }
  // TODO(rajendrant): Also allow the throttle to be stopped when LiteMode gets
  // disabled or ECT worsens. This logic should probably be in the browser
  // process.
  if (!IsLiteVideoEnabled())
    return nullptr;

  auto* lite_video_hint_agent = GetLiteVideoHintAgent(render_frame_id);
  if (lite_video_hint_agent && lite_video_hint_agent->HasLiteVideoHint())
    return std::make_unique<LiteVideoURLLoaderThrottle>(render_frame_id);

  return nullptr;
}

LiteVideoURLLoaderThrottle::LiteVideoURLLoaderThrottle(int render_frame_id)
    : render_frame_id_(render_frame_id) {
  DCHECK(IsLiteVideoEnabled());
}

LiteVideoURLLoaderThrottle::~LiteVideoURLLoaderThrottle() {
  // Existence of |response_delay_timer_| indicates throttling has been
  // attempted on this media response. Remove the throttle on this case.
  if (response_delay_timer_) {
    DCHECK(render_frame_id_);
    auto* lite_video_hint_agent = GetLiteVideoHintAgent(render_frame_id_);
    if (lite_video_hint_agent)
      lite_video_hint_agent->RemoveThrottle(this);
  }
}

void LiteVideoURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (!response_head || !response_head->headers)
    return;
  // Do not throttle on 4xx, 5xx failures.
  if (response_head->headers->response_code() != 200)
    return;
  if (!response_head->network_accessed ||
      response_head->was_fetched_via_cache) {
    return;
  }
  if (!base::StartsWith(response_head->mime_type, "video/",
                        base::CompareCase::SENSITIVE)) {
    return;
  }

  auto* lite_video_hint_agent = GetLiteVideoHintAgent(render_frame_id_);
  if (!lite_video_hint_agent)
    return;

  auto received_bytes = GetContentLength(*response_head);
  if (received_bytes)
    lite_video_hint_agent->NotifyThrottledDataUse(*received_bytes);

  auto latency = lite_video_hint_agent->CalculateLatencyForResourceResponse(
      *response_head);
  if (latency.is_zero())
    return;

  UMA_HISTOGRAM_TIMES("LiteVideo.URLLoader.ThrottleLatency", latency);

  *defer = true;
  // The timer may have already started and running, and the below restart will
  // lose that elapsed time. However the elapsed time will be small since this
  // case happens if some other url loader throttle is restarting the request.
  response_delay_timer_ = std::make_unique<base::OneShotTimer>();
  response_delay_timer_->Start(
      FROM_HERE, latency,
      base::BindOnce(&LiteVideoURLLoaderThrottle::ResumeThrottledMediaResponse,
                     base::Unretained(this)));

  lite_video_hint_agent->AddThrottle(this);
}

void LiteVideoURLLoaderThrottle::ResumeIfThrottled() {
  if (response_delay_timer_ && response_delay_timer_->IsRunning()) {
    response_delay_timer_->Stop();
    ResumeThrottledMediaResponse();
  }
}

void LiteVideoURLLoaderThrottle::ResumeThrottledMediaResponse() {
  DCHECK(!response_delay_timer_->IsRunning());
  delegate_->Resume();
}

void LiteVideoURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace lite_video
