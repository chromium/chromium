// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/renderer/previews/resource_loading_hints_agent.h"
#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
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
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace subresource_redirect {

namespace {

// Returns the decider for the render frame
PublicResourceDeciderAgent* GetPublicResourceDeciderAgent(int render_frame_id) {
  return PublicResourceDeciderAgent::Get(
      content::RenderFrame::FromRoutingID(render_frame_id));
}

// Records the per image load metrics.
void RecordMetricsOnLoadFinished(
    LoginRobotsCompressionMetrics* login_robots_compression_metrics,
    SubresourceRedirectResult redirect_result,
    uint64_t content_length,
    base::Optional<float> ofcl) {
  if (login_robots_compression_metrics) {
    login_robots_compression_metrics->RecordMetricsOnLoadFinished(
        redirect_result, content_length, ofcl);
  }
}

// Returns whether the redirect state is in some terminal state.
bool IsTerminalRedirectState(
    PublicResourceDeciderRedirectState redirect_state) {
  switch (redirect_state) {
    case PublicResourceDeciderRedirectState::kNone:
    case PublicResourceDeciderRedirectState::kRedirectAttempted:
    case PublicResourceDeciderRedirectState::kRedirectNotAllowedByDecider:
    case PublicResourceDeciderRedirectState::kRedirectFailed:
    case PublicResourceDeciderRedirectState::kRedirectAllowed:
      // This enum is not used in this file.
      return true;
    case PublicResourceDeciderRedirectState::kRedirectDecisionPending:
      return false;
  }
}

}  // namespace

// static
std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
    const blink::WebURLRequest& request,
    int render_frame_id) {
  if (!ShouldEnablePublicImageHintsBasedCompression() &&
      !ShouldEnableLoginRobotsCheckedImageCompression()) {
    return nullptr;
  }
  if (request.GetRequestDestination() ==
          network::mojom::RequestDestination::kImage &&
      request.Url().ProtocolIs(url::kHttpsScheme) &&
      blink::WebNetworkStateNotifier::SaveDataEnabled() &&
      request.GetRequestContext() !=
          blink::mojom::RequestContextType::FAVICON) {
    return base::WrapUnique<SubresourceRedirectURLLoaderThrottle>(
        new SubresourceRedirectURLLoaderThrottle(
            render_frame_id, request.GetPreviewsState() &
                                 blink::PreviewsTypes::kSubresourceRedirectOn));
  }
  return nullptr;
}

SubresourceRedirectURLLoaderThrottle::SubresourceRedirectURLLoaderThrottle(
    int render_frame_id,
    bool allowed_to_redirect)
    : render_frame_id_(render_frame_id) {
  DCHECK(ShouldEnablePublicImageHintsBasedCompression() ||
         ShouldEnableLoginRobotsCheckedImageCompression());
  redirect_result_ =
      allowed_to_redirect
          ? SubresourceRedirectResult::kRedirectable
          : SubresourceRedirectResult::kIneligibleBlinkDisallowed;
  if (!ShouldRecordLoginRobotsUkmMetrics())
    return;
  if (!ShouldEnableLoginRobotsCheckedImageCompression())
    return;
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  if (!render_frame || !render_frame->GetWebFrame())
    return;
  auto* public_resource_decider_agent =
      GetPublicResourceDeciderAgent(render_frame_id);
  if (!public_resource_decider_agent)
    return;
  login_robots_compression_metrics_ = LoginRobotsCompressionMetrics(
      render_frame->GetWebFrame()->GetDocument().GetUkmSourceId(),
      public_resource_decider_agent->GetNavigationStartTime());
}

SubresourceRedirectURLLoaderThrottle::~SubresourceRedirectURLLoaderThrottle() =
    default;

void SubresourceRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_EQ(request->destination, network::mojom::RequestDestination::kImage);
  DCHECK(request->url.SchemeIs(url::kHttpsScheme));

  if (redirect_result_ != SubresourceRedirectResult::kRedirectable)
    return;

  // Do not redirect if its already a litepage subresource.
  if (IsCompressionServerOrigin(request->url))
    return;

  if (!ShouldCompressRedirectSubresource())
    return;

  if (login_robots_compression_metrics_)
    login_robots_compression_metrics_->NotifyRequestStart();

  auto* public_resource_decider_agent =
      GetPublicResourceDeciderAgent(render_frame_id_);
  if (!public_resource_decider_agent)
    return;

  auto redirect_result =
      public_resource_decider_agent->ShouldRedirectSubresource(
          request->url, base::BindOnce(&SubresourceRedirectURLLoaderThrottle::
                                           NotifyRedirectDeciderDecision,
                                       weak_ptr_factory_.GetWeakPtr()));
  if (!redirect_result) {
    // Decision cannot be made yet. Defer the subresource and change the URL to
    // compression server URL. The NotifyRedirectDeciderDecision callback will
    // continue with compression or disable compression by resetting to original
    // URL.
    redirect_state_ =
        PublicResourceDeciderRedirectState::kRedirectDecisionPending;
    *defer = true;
    request->url = GetSubresourceURLForURL(request->url);
    return;
  }

  // The decider decision has been made.
  if (login_robots_compression_metrics_)
    login_robots_compression_metrics_->NotifyRequestSent();
  *defer = false;
  redirect_result_ = *redirect_result;
  if (redirect_result_ != SubresourceRedirectResult::kRedirectable) {
    redirect_state_ =
        PublicResourceDeciderRedirectState::kRedirectNotAllowedByDecider;
    return;
  }

  // Redirect is allowed.
  redirect_state_ = PublicResourceDeciderRedirectState::kRedirectAttempted;
  request->url = GetSubresourceURLForURL(request->url);
  StartRedirectTimeoutTimer();
}

const char*
SubresourceRedirectURLLoaderThrottle::NameForLoggingWillStartRequest() {
  return "SubresourceRedirectThrottle";
}

void SubresourceRedirectURLLoaderThrottle::NotifyRedirectDeciderDecision(
    SubresourceRedirectResult redirect_result) {
  DCHECK_EQ(PublicResourceDeciderRedirectState::kRedirectDecisionPending,
            redirect_state_);
  redirect_result_ = redirect_result;
  if (login_robots_compression_metrics_)
    login_robots_compression_metrics_->NotifyRequestSent();

  if (redirect_result_ != SubresourceRedirectResult::kRedirectable) {
    // Restart the fetch to the original URL.
    redirect_state_ =
        PublicResourceDeciderRedirectState::kRedirectNotAllowedByDecider;
    delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
    delegate_->Resume();
    return;
  }

  // Redirect is allowed.
  redirect_state_ = PublicResourceDeciderRedirectState::kRedirectAttempted;
  delegate_->Resume();
  StartRedirectTimeoutTimer();
}

void SubresourceRedirectURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  // Check if the redirect is in some terminal state.
  DCHECK(IsTerminalRedirectState(redirect_state_));
  if (redirect_state_ ==
          PublicResourceDeciderRedirectState::kRedirectAttempted &&
      redirect_timeout_timer_) {
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
  DCHECK(IsTerminalRedirectState(redirect_state_));
  if (redirect_state_ != PublicResourceDeciderRedirectState::kRedirectAttempted)
    return;
  DCHECK(ShouldCompressRedirectSubresource());
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
  redirect_result_ = SubresourceRedirectResult::kIneligibleRedirectFailed;

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
    if (auto* public_resource_decider_agent =
            GetPublicResourceDeciderAgent(render_frame_id_)) {
      public_resource_decider_agent->NotifyCompressedResourceFetchFailed(
          retry_after);
    }
  }

  // Non 2XX responses from the compression server need to have unaltered
  // requests sent to the original resource.
  redirect_state_ = PublicResourceDeciderRedirectState::kRedirectFailed;
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
}

void SubresourceRedirectURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK(IsTerminalRedirectState(redirect_state_));
  // If response was not from the compression server, don't record any
  // metrics.
  if (!response_url.is_valid())
    return;
  if (response_head->was_fetched_via_cache)
    return;
  int64_t content_length = response_head->headers->GetContentLength();
  if (content_length < 0)
    return;

  if (auto* public_resource_decider_agent =
          GetPublicResourceDeciderAgent(render_frame_id_)) {
    public_resource_decider_agent->RecordMetricsOnLoadFinished(
        response_url, content_length, redirect_result_);
  }

  if (redirect_state_ !=
      PublicResourceDeciderRedirectState::kRedirectAttempted) {
    RecordMetricsOnLoadFinished(
        base::OptionalOrNullptr(login_robots_compression_metrics_),
        redirect_result_, content_length, base::nullopt);
    return;
  }
  DCHECK(ShouldCompressRedirectSubresource());

  // Record that the server responded.
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true);

  // If compression was unsuccessful don't try and record compression percent.
  if (response_head->headers->response_code() != 200) {
    RecordMetricsOnLoadFinished(
        base::OptionalOrNullptr(login_robots_compression_metrics_),
        redirect_result_, content_length, base::nullopt);
    return;
  }

  int64_t ofcl =
      static_cast<float>(data_reduction_proxy::GetDataReductionProxyOFCL(
          response_head->headers.get()));

  // If |ofcl| is missing don't compute compression percent.
  if (ofcl <= 0) {
    RecordMetricsOnLoadFinished(
        base::OptionalOrNullptr(login_robots_compression_metrics_),
        redirect_result_, content_length, base::nullopt);
    return;
  }

  UMA_HISTOGRAM_PERCENTAGE(
      "SubresourceRedirect.DidCompress.CompressionPercent",
      static_cast<int>(100 -
                       ((content_length / static_cast<float>(ofcl)) * 100)));

  UMA_HISTOGRAM_COUNTS_1M("SubresourceRedirect.DidCompress.BytesSaved",
                          ofcl - content_length);
  RecordMetricsOnLoadFinished(
      base::OptionalOrNullptr(login_robots_compression_metrics_),
      redirect_result_, content_length, ofcl);
}

void SubresourceRedirectURLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status,
    bool* defer) {
  if (redirect_state_ != PublicResourceDeciderRedirectState::kRedirectAttempted)
    return;
  DCHECK(ShouldCompressRedirectSubresource());
  redirect_result_ = SubresourceRedirectResult::kIneligibleRedirectFailed;

  // If the server fails, restart the request to the original resource, and
  // record it.
  redirect_state_ = PublicResourceDeciderRedirectState::kRedirectFailed;
  redirect_timeout_timer_.reset();
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false);
}

void SubresourceRedirectURLLoaderThrottle::StartRedirectTimeoutTimer() {
  DCHECK(!redirect_timeout_timer_);
  redirect_timeout_timer_ = std::make_unique<base::OneShotTimer>();
  redirect_timeout_timer_->Start(
      FROM_HERE, GetCompressionRedirectTimeout(),
      base::BindOnce(&SubresourceRedirectURLLoaderThrottle::OnRedirectTimeout,
                     base::Unretained(this)));
}

void SubresourceRedirectURLLoaderThrottle::OnRedirectTimeout() {
  DCHECK_EQ(PublicResourceDeciderRedirectState::kRedirectAttempted,
            redirect_state_);
  redirect_state_ = PublicResourceDeciderRedirectState::kRedirectFailed;
  delegate_->RestartWithURLResetAndFlagsNow(net::LOAD_NORMAL);
  if (auto* public_resource_decider_agent =
          GetPublicResourceDeciderAgent(render_frame_id_)) {
    public_resource_decider_agent->NotifyCompressedResourceFetchFailed(
        base::TimeDelta());
  }
  UMA_HISTOGRAM_BOOLEAN("SubresourceRedirect.CompressionFetchTimeout", true);
}

void SubresourceRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect
