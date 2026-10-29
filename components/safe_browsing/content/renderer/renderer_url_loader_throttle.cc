// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"  // nogncheck
#endif

namespace safe_browsing {

namespace {

// Returns true if the URL is known to be safe. We also require that this URL
// never redirects to a potentially unsafe URL, because the redirected URLs are
// also skipped if this function returns true.
bool KnownSafeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle() = default;

#if BUILDFLAG(ENABLE_EXTENSIONS)
RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojo::PendingRemote<mojom::ExtensionWebRequestReporter>
        extension_web_request_reporter)
    : extension_web_request_reporter_(
          std::move(extension_web_request_reporter)) {}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() = default;

void RendererURLLoaderThrottle::DetachFromCurrentSequence() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Pass the pipe to the ExtensionWebRequestReporter interface to be bound to
  // a different sequence.
  if (extension_web_request_reporter_) {
    pending_extension_web_request_reporter_ =
        extension_web_request_reporter_.Unbind();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void RendererURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  MaybeSendExtensionWebRequestData(request);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  base::UmaHistogramEnumeration(
      "SafeBrowsing.RendererThrottle.RequestDestination", request->destination);

  if (KnownSafeUrl(request->url)) {
    return;
  }

  VLOG(2) << __func__ << " : Skipping: " << request->url << " : "
          << request->destination;
  CHECK_NE(request->destination, network::mojom::RequestDestination::kDocument);
}

void RendererURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* /* defer */,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  BindExtensionWebRequestReporterPipeIfDetached();
  // Send redirected request data to the browser if request originated from an
  // extension and the redirected url is HTTP/HTTPS scheme only.
  if (!origin_extension_id_.empty() &&
      redirect_info->new_url.SchemeIsHTTPOrHTTPS()) {
    extension_web_request_reporter_->SendWebRequestData(
        origin_extension_id_, redirect_info->new_url,
        mojom::WebRequestProtocolType::kHttpHttps,
        initiated_from_content_script_
            ? mojom::WebRequestContactInitiatorType::kContentScript
            : mojom::WebRequestContactInitiatorType::kExtension);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void RendererURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.RendererThrottle.IsCheckCompletedOnProcessResponse",
      /*sample=*/true);
}

const char* RendererURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingRendererThrottle";
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void RendererURLLoaderThrottle::
    BindExtensionWebRequestReporterPipeIfDetached() {
  if (pending_extension_web_request_reporter_) {
    extension_web_request_reporter_.Bind(
        std::move(pending_extension_web_request_reporter_));
  }
}

void RendererURLLoaderThrottle::MaybeSendExtensionWebRequestData(
    network::ResourceRequest* request) {
  BindExtensionWebRequestReporterPipeIfDetached();
  // Skip if request destination isn't HTTP/HTTPS (ex. extension scheme).
  if (!request->url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // Populate |origin_extension_id_| if request is initiated from an extension
  // page/service worker or content script.
  if (request->request_initiator &&
      request->request_initiator->scheme() == extensions::kExtensionScheme) {
    origin_extension_id_ = request->request_initiator->host();
  } else if (request->isolated_world_origin &&
             request->isolated_world_origin->scheme() ==
                 extensions::kExtensionScheme) {
    origin_extension_id_ = request->isolated_world_origin->host();
    initiated_from_content_script_ = true;
  }

  // Send data only if |origin_extension_id_| is populated, which means the
  // request originated from an extension.
  if (!origin_extension_id_.empty()) {
    extension_web_request_reporter_->SendWebRequestData(
        origin_extension_id_, request->url,
        mojom::WebRequestProtocolType::kHttpHttps,
        initiated_from_content_script_
            ? mojom::WebRequestContactInitiatorType::kContentScript
            : mojom::WebRequestContactInitiatorType::kExtension);
  }
}
#endif

}  // namespace safe_browsing
