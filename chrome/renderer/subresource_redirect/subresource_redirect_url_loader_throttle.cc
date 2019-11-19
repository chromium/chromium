// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_experiments.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

// static
std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
    const blink::WebURLRequest& request,
    content::ResourceType resource_type) {
  if (base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
      resource_type == content::ResourceType::kImage &&
      (request.GetPreviewsState() &
       blink::WebURLRequest::kSubresourceRedirectOn) &&
      request.Url().ProtocolIs(url::kHttpsScheme)) {
    // TODO(rajendrant): Verify that data saver is enabled as well, to not
    // trigger the subresource redirect for incognito profiles.
    return base::WrapUnique<SubresourceRedirectURLLoaderThrottle>(
        new SubresourceRedirectURLLoaderThrottle());
  }
  return nullptr;
}

SubresourceRedirectURLLoaderThrottle::SubresourceRedirectURLLoaderThrottle() =
    default;
SubresourceRedirectURLLoaderThrottle::~SubresourceRedirectURLLoaderThrottle() =
    default;

void SubresourceRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  DCHECK_EQ(request->resource_type,
            static_cast<int>(content::ResourceType::kImage));
  DCHECK(request->previews_state &
         content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON);
  DCHECK(request->url.SchemeIs(url::kHttpsScheme));

  // Image subresources that have paths that do not end in one of the
  // following common formats are commonly single pixel images that will not
  // benefit from being sent to the compression server.
  if (!ShouldIncludeMediaSuffix(request->url))
    return;

  request->url = GetSubresourceURLForURL(request->url);
  *defer = false;
}

void SubresourceRedirectURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      static_cast<net::HttpStatusCode>(response_head.headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);
}

void SubresourceRedirectURLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  // If response was not from the compression server, don't restart it.
  if (!response_url.is_valid())
    return;
  auto compression_server = GetSubresourceRedirectOrigin();
  if (!response_url.DomainIs(compression_server.host()))
    return;
  if (response_url.EffectiveIntPort() != compression_server.port())
    return;
  if (response_url.scheme() != compression_server.scheme())
    return;

  // Log all response codes, from the compression server.
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      static_cast<net::HttpStatusCode>(response_head.headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);

  // Do nothing with 2XX responses, as these requests were handled
  // correctly by the compression server.
  if ((response_head.headers->response_code() >= 200 &&
       response_head.headers->response_code() <= 299) ||
      response_head.headers->response_code() == 304) {
    return;
  }

  // Non 2XX responses from the compression server need to have unaltered
  // requests sent to the original resource.
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
}

void SubresourceRedirectURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // If response was not from the compression server, don't record any metrics.
  if (!response_url.is_valid())
    return;
  auto compression_server = GetSubresourceRedirectOrigin();
  if (!response_url.DomainIs(compression_server.host()))
    return;
  if (response_url.EffectiveIntPort() != compression_server.port())
    return;
  if (response_url.scheme() != compression_server.scheme())
    return;

  // Record that the server responded.
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true);

  // If compression was unsuccessful don't try and record compression percent.
  if (response_head->headers->response_code() != 200)
    return;

  float content_length =
      static_cast<float>(response_head->headers->GetContentLength());

  float ofcl =
      static_cast<float>(data_reduction_proxy::GetDataReductionProxyOFCL(
          response_head->headers.get()));

  // If either |content_length| or |ofcl| are missing don't compute compression
  // percent.
  if (content_length < 0.0 || ofcl <= 0.0)
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
  // If the server fails, restart the request to the original resource, and
  // record it.
  delegate_->RestartWithURLResetAndFlags(net::LOAD_NORMAL);
  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false);
}

void SubresourceRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect
