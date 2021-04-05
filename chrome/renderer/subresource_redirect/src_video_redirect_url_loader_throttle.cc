// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/src_video_redirect_url_loader_throttle.h"

#include "base/metrics/histogram_macros_local.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace subresource_redirect {

namespace {

// Returns the decider for the render frame
PublicResourceDeciderAgent* GetPublicResourceDeciderAgent(int render_frame_id) {
  return PublicResourceDeciderAgent::Get(
      content::RenderFrame::FromRoutingID(render_frame_id));
}

// Returns the full content length of the response, either from the range, or
// content-length response headers, or the total body length.
base::Optional<uint64_t> GetFullContentLength(
    const network::mojom::URLResponseHead& response_head) {
  if (response_head.headers->response_code() == net::HTTP_PARTIAL_CONTENT) {
    // Parse the full length from range response.
    int64_t first_byte, last_byte, total_length;
    if (response_head.headers->GetContentRangeFor206(&first_byte, &last_byte,
                                                     &total_length)) {
      return total_length;
    }
  } else if ((response_head.headers->response_code() >= 200 &&
              response_head.headers->response_code() <= 299)) {
    if (response_head.content_length > 0)
      return static_cast<uint64_t>(response_head.content_length);
    if (response_head.encoded_body_length > 0)
      return static_cast<uint64_t>(response_head.encoded_body_length);
  }
  return base::nullopt;
}

}  // namespace

// static
std::unique_ptr<SrcVideoRedirectURLLoaderThrottle>
SrcVideoRedirectURLLoaderThrottle::MaybeCreateThrottle(
    const blink::WebURLRequest& request,
    int render_frame_id) {
  if (!ShouldRecordLoginRobotsCheckedSrcVideoMetrics())
    return nullptr;
  if (!blink::WebNetworkStateNotifier::SaveDataEnabled())
    return nullptr;
  if (request.GetRequestDestination() !=
      network::mojom::RequestDestination::kVideo)
    return nullptr;
  if (!request.Url().ProtocolIs(url::kHttpsScheme) &&
      !request.Url().ProtocolIs(url::kHttpScheme)) {
    return nullptr;
  }
  if (!(request.GetPreviewsState() &
        blink::PreviewsTypes::kSrcVideoRedirectOn)) {
    return nullptr;
  }

  return base::WrapUnique<SrcVideoRedirectURLLoaderThrottle>(
      new SrcVideoRedirectURLLoaderThrottle(render_frame_id));
}

SrcVideoRedirectURLLoaderThrottle::SrcVideoRedirectURLLoaderThrottle(
    int render_frame_id)
    : render_frame_id_(render_frame_id) {
  DCHECK(ShouldRecordLoginRobotsCheckedSrcVideoMetrics());
  redirect_result_ = SubresourceRedirectResult::kRedirectable;
}

SrcVideoRedirectURLLoaderThrottle::~SrcVideoRedirectURLLoaderThrottle() =
    default;

void SrcVideoRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_EQ(request->destination, network::mojom::RequestDestination::kVideo);
  DCHECK(request->url.SchemeIs(url::kHttpsScheme) ||
         request->url.SchemeIs(url::kHttpScheme));
  DCHECK_EQ(redirect_result_, SubresourceRedirectResult::kRedirectable);
  DCHECK_EQ(redirect_state_, PublicResourceDeciderRedirectState::kNone);

  // Do not redirect if its already a litepage subresource.
  if (IsCompressionServerOrigin(request->url))
    return;

  auto* public_resource_decider_agent =
      GetPublicResourceDeciderAgent(render_frame_id_);
  if (!public_resource_decider_agent)
    return;

  auto redirect_result =
      public_resource_decider_agent->ShouldRedirectSubresource(
          request->url,
          base::BindOnce(
              &SrcVideoRedirectURLLoaderThrottle::NotifyRedirectDeciderDecision,
              weak_ptr_factory_.GetWeakPtr()));
  if (!redirect_result) {
    // Decision cannot be made yet. Wait for the NotifyRedirectDeciderDecision
    // callback.
    redirect_state_ =
        PublicResourceDeciderRedirectState::kRedirectDecisionPending;
    return;
  }

  // The decider decision has been made.
  redirect_result_ = *redirect_result;
  redirect_state_ =
      redirect_result_ == SubresourceRedirectResult::kRedirectable
          ? PublicResourceDeciderRedirectState::kRedirectAllowed
          : PublicResourceDeciderRedirectState::kRedirectNotAllowedByDecider;
}

void SrcVideoRedirectURLLoaderThrottle::NotifyRedirectDeciderDecision(
    SubresourceRedirectResult redirect_result) {
  DCHECK_EQ(PublicResourceDeciderRedirectState::kRedirectDecisionPending,
            redirect_state_);
  redirect_result_ = redirect_result;
  redirect_state_ =
      redirect_result_ == SubresourceRedirectResult::kRedirectable
          ? PublicResourceDeciderRedirectState::kRedirectAllowed
          : PublicResourceDeciderRedirectState::kRedirectNotAllowedByDecider;
}

void SrcVideoRedirectURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (!response_url.is_valid())
    return;
  if (response_head->was_fetched_via_cache)
    return;

  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id_);
  if (!render_frame || !render_frame->GetWebFrame())
    return;

  if (redirect_state_ ==
      PublicResourceDeciderRedirectState::kRedirectDecisionPending) {
    // When the robots rules is not retrieved yet, treat that as a timeout.
    redirect_result_ = SubresourceRedirectResult::kIneligibleRobotsTimeout;
  }
  auto full_content_length = GetFullContentLength(*response_head);

  ukm::builders::SubresourceRedirect_PublicSrcVideoCompression
      src_video_compression(
          render_frame->GetWebFrame()->GetDocument().GetUkmSourceId());
  src_video_compression.SetSubresourceRedirectResult(
      static_cast<int64_t>(redirect_result_));
  src_video_compression.SetFullContentLength(
      ukm::GetExponentialBucketMin(full_content_length.value_or(0), 1.3));

  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder =
      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  src_video_compression.Record(ukm_recorder.get());

  LOCAL_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult",
      redirect_result_);
  if (redirect_result_ == SubresourceRedirectResult::kRedirectable) {
    DCHECK_EQ(PublicResourceDeciderRedirectState::kRedirectAllowed,
              redirect_state_);
    LOCAL_HISTOGRAM_COUNTS_1000000(
        "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes",
        full_content_length.value_or(0));
  }
}

void SrcVideoRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect
