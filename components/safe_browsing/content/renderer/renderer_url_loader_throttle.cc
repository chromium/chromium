// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
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

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token)
    : safe_browsing_(safe_browsing),
      frame_token_(local_frame_token.CopyAsOptional()) {}

#if BUILDFLAG(ENABLE_EXTENSIONS)
RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    mojom::ExtensionWebRequestReporter* extension_web_request_reporter)
    : safe_browsing_(safe_browsing),
      frame_token_(local_frame_token.CopyAsOptional()),
      extension_web_request_reporter_(extension_web_request_reporter) {}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() {
  if (deferred_)
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
}

void RendererURLLoaderThrottle::DetachFromCurrentSequence() {
  // Create a new pipe to the SafeBrowsing interface that can be bound to a
  // different sequence.
  safe_browsing_->Clone(
      safe_browsing_pending_remote_.InitWithNewPipeAndPassReceiver());
  safe_browsing_ = nullptr;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Create a new pipe to the ExtensionWebRequestReporter interface that can be
  // bound to a different sequence.
  extension_web_request_reporter_->Clone(
      extension_web_request_reporter_pending_remote_
          .InitWithNewPipeAndPassReceiver());
  extension_web_request_reporter_ = nullptr;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void RendererURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

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
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

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

  if (!url_checker_) {
    DCHECK_EQ(0u, pending_checks_);
    return;
  }

  pending_checks_++;
  url_checker_->CheckUrl(
      redirect_info->new_url, redirect_info->new_method,
      base::BindOnce(&RendererURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void RendererURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  bool check_completed = (pending_checks_ == 0);
  base::UmaHistogramBoolean(
      "SafeBrowsing.RendererThrottle.IsCheckCompletedOnProcessResponse",
      check_completed);

  if (check_completed) {
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  *defer = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this), "original_url",
                                    original_url_.spec());
}

const char* RendererURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingRendererThrottle";
}

void RendererURLLoaderThrottle::OnCheckUrlResult(
    bool proceed,
    bool showed_interstitial) {
  // When this is the callback of safe_browsing_->CreateCheckerAndCheck(), it is
  // possible that we get here after a check with |url_checker_| has completed
  // and blocked the request.
  if (blocked_ || !url_checker_)
    return;

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (proceed) {
    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    url_checker_.reset();
    pending_checks_ = 0;
    // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
    // an error page either.
    delegate_->CancelWithError(
        showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
        kCustomCancelReasonForURLLoader);
  }
}

void RendererURLLoaderThrottle::OnMojoDisconnect() {
  DCHECK(!blocked_);

  // If a service-side disconnect happens, treat all URLs as if they are safe.
  url_checker_.reset();

  pending_checks_ = 0;

  if (deferred_) {
    deferred_ = false;
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
    delegate_->Resume();
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void RendererURLLoaderThrottle::
    BindExtensionWebRequestReporterPipeIfDetached() {
  if (extension_web_request_reporter_pending_remote_.is_valid()) {
    extension_web_request_reporter_remote_.Bind(
        std::move(extension_web_request_reporter_pending_remote_));
    extension_web_request_reporter_ =
        extension_web_request_reporter_remote_.get();
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
