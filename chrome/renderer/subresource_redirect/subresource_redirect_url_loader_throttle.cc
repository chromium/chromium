// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/renderer/previews/resource_loading_hints_agent.h"
#include "chrome/renderer/subresource_redirect/public_image_hints_url_loader_throttle.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/renderer/render_frame.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

namespace {

// Whether the url points to compressed server origin.
bool IsCompressionServerOrigin(const GURL& url) {
  auto compression_server = GetSubresourceRedirectOrigin();
  return url.DomainIs(compression_server.host()) &&
         (url.EffectiveIntPort() == compression_server.port()) &&
         (url.scheme() == compression_server.scheme());
}

}  // namespace

// static
std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
    const blink::WebURLRequest& request,
    int render_frame_id) {
  if (IsPublicImageHintsBasedCompressionEnabled() &&
      request.GetRequestDestination() ==
          network::mojom::RequestDestination::kImage &&
      request.Url().ProtocolIs(url::kHttpsScheme) &&
      blink::WebNetworkStateNotifier::SaveDataEnabled() &&
      request.GetRequestContext() !=
          blink::mojom::RequestContextType::FAVICON) {
    return base::WrapUnique<SubresourceRedirectURLLoaderThrottle>(
        new PublicImageHintsURLLoaderThrottle(
            render_frame_id, request.GetPreviewsState() &
                                 blink::PreviewsTypes::kSubresourceRedirectOn));
  }
  return nullptr;
}

SubresourceRedirectURLLoaderThrottle::SubresourceRedirectURLLoaderThrottle(
    int render_frame_id)
    : render_frame_id_(render_frame_id) {}

SubresourceRedirectURLLoaderThrottle::~SubresourceRedirectURLLoaderThrottle() =
    default;

void SubresourceRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(IsPublicImageHintsBasedCompressionEnabled());
  DCHECK_EQ(request->destination, network::mojom::RequestDestination::kImage);
  DCHECK(request->url.SchemeIs(url::kHttpsScheme));

  // Do not redirect if its already a litepage subresource.
  if (IsCompressionServerOrigin(request->url))
    return;

  if (!ShouldRedirectImage(request->url))
    return;

  if (!ShouldCompressionServerRedirectSubresource())
    return;

  request->url = GetSubresourceURLForURL(request->url);
  did_redirect_compressed_origin_ = true;
  *defer = false;

  DCHECK(!redirect_timeout_timer_);
  redirect_timeout_timer_ = std::make_unique<base::OneShotTimer>();
  redirect_timeout_timer_->Start(
      FROM_HERE, GetCompressionRedirectTimeout(),
      base::BindOnce(&SubresourceRedirectURLLoaderThrottle::OnRedirectTimeout,
                     base::Unretained(this)));
}

void SubresourceRedirectURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (did_redirect_compressed_origin_ && redirect_timeout_timer_) {
    redirect_timeout_timer_->Start(
        FROM_HERE, GetCompressionRedirectTimeout(),
        base::BindOnce(&SubresourceRedirectURLLoaderThrottle::OnRedirectTimeout,
                       base::Unretained(this)));
  }
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      static_cast<net::HttpStatusCode>(response_head.headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);
}

void SubresourceRedirectURLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  if (!did_redirect_compressed_origin_)
    return;
  DCHECK(ShouldCompressionServerRedirectSubresource());
  // If response was not from the compression server, don't restart it.
  if (!response_url.is_valid())
    return;

  // Log all response codes, from the compression server.
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      static_cast<net::HttpStatusCode>(response_head.headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);
  redirect_timeout_timer_.reset();

  // Do nothing with 2XX responses, as these requests were handled
  // correctly by the compression server.
  if ((response_head.headers->response_code() >= 200 &&
       response_head.headers->response_code() <= 299) ||
      response_head.headers->response_code() == 304) {
    return;
  }
  OnRedirectedLoadCompleteWithError();

  // 503 response code indicates loadshed from the compression server. Notify
  // the browser process which will bypass subresource redirect for subsequent
  // page loads. Retry-After response header may mention the bypass duration,
  // otherwise the browser will choose a random duration.
  if (response_head.headers->response_code() == 503) {
    std::string retry_after_string;
    base::TimeDelta retry_after;
    if (response_head.headers->EnumerateHeader(nullptr, "Retry-After",
                                               &retry_after_string)) {
      net::HttpUtil::ParseRetryAfterHeader(retry_after_string,
                                           base::Time::Now(), &retry_after);
    }
    if (auto* subresource_redirect_hints_agent =
            SubresourceRedirectHintsAgent::Get(GetRenderFrame())) {
      subresource_redirect_hints_agent->NotifyHttpsImageCompressionFetchFailed(
          retry_after);
    }
  }

  // Non 2XX responses from the compression server need to have unaltered
  // requests sent to the original resource.
  did_redirect_compressed_origin_ = false;
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
}

void SubresourceRedirectURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // If response was not from the compression server, don't record any metrics.
  if (!response_url.is_valid())
    return;
  if (response_head->was_fetched_via_cache)
    return;
  int64_t content_length = response_head->headers->GetContentLength();
  if (content_length < 0)
    return;

  RecordMetricsOnLoadFinished(response_url, content_length);

  if (!did_redirect_compressed_origin_)
    return;
  DCHECK(ShouldCompressionServerRedirectSubresource());

  // Record that the server responded.
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true);

  // If compression was unsuccessful don't try and record compression percent.
  if (response_head->headers->response_code() != 200)
    return;

  float ofcl =
      static_cast<float>(data_reduction_proxy::GetDataReductionProxyOFCL(
          response_head->headers.get()));

  // If |ofcl| is missing don't compute compression percent.
  if (ofcl <= 0.0)
    return;

  UMA_HISTOGRAM_PERCENTAGE(
      "SubresourceRedirect.DidCompress.CompressionPercent",
      static_cast<int>(100 - ((content_length / ofcl) * 100)));

  UMA_HISTOGRAM_COUNTS_1M("SubresourceRedirect.DidCompress.BytesSaved",
                          static_cast<int>(ofcl - content_length));
}

void SubresourceRedirectURLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status,
    bool* defer) {
  if (!did_redirect_compressed_origin_)
    return;
  DCHECK(ShouldCompressionServerRedirectSubresource());
  OnRedirectedLoadCompleteWithError();

  // If the server fails, restart the request to the original resource, and
  // record it.
  did_redirect_compressed_origin_ = false;
  redirect_timeout_timer_.reset();
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false);
}

void SubresourceRedirectURLLoaderThrottle::OnRedirectTimeout() {
  DCHECK(did_redirect_compressed_origin_);
  did_redirect_compressed_origin_ = false;
  delegate_->RestartWithURLResetAndFlagsNow(net::LOAD_NORMAL);
  if (auto* subresource_redirect_hints_agent =
          SubresourceRedirectHintsAgent::Get(GetRenderFrame())) {
    subresource_redirect_hints_agent->NotifyHttpsImageCompressionFetchFailed(
        base::TimeDelta());
  }
  UMA_HISTOGRAM_BOOLEAN("SubresourceRedirect.CompressionFetchTimeout", true);
}

void SubresourceRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect
