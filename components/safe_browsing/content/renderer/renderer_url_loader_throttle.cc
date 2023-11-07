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

constexpr char kFromCacheUmaSuffix[] = ".FromCache";
constexpr char kFromNetworkUmaSuffix[] = ".FromNetwork";

// Returns true if the URL is known to be safe. We also require that this URL
// never redirects to a potentially unsafe URL, because the redirected URLs are
// also skipped if this function returns true.
bool KnownSafeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}

void LogTotalDelay3Metrics(base::TimeDelta total_delay) {
  base::UmaHistogramTimes("SafeBrowsing.RendererThrottle.TotalDelay3",
                          total_delay);
}

void LogTotalDelay2MetricsWithResponseType(bool is_response_from_cache,
                                           base::TimeDelta total_delay) {
  base::UmaHistogramTimes(
      base::StrCat({"SafeBrowsing.RendererThrottle.TotalDelay2",
                    is_response_from_cache ? kFromCacheUmaSuffix
                                           : kFromNetworkUmaSuffix}),
      total_delay);
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
    LogTotalDelay3Metrics(base::TimeDelta());
    return;
  }

  static const base::NoDestructor<
      std::unordered_set<network::mojom::RequestDestination>>
      request_destinations_to_skip{{network::mojom::RequestDestination::kStyle,
                                    network::mojom::RequestDestination::kImage,
                                    network::mojom::RequestDestination::kFont}};
  if (base::FeatureList::IsEnabled(kSafeBrowsingSkipSubresources) ||
      (base::Contains(*request_destinations_to_skip, request->destination) &&
       base::FeatureList::IsEnabled(kSafeBrowsingSkipImageCssFont))) {
    VLOG(2) << __func__ << " : Skipping: " << request->url << " : "
            << request->destination;
    DCHECK_NE(request->destination,
              network::mojom::RequestDestination::kDocument);
    LogTotalDelay3Metrics(base::TimeDelta());
    base::UmaHistogramEnumeration(
        "SafeBrowsing.RendererThrottle.RequestDestination.Skipped",
        request->destination);
    return;
  }

  base::UmaHistogramEnumeration(
      "SafeBrowsing.RendererThrottle.RequestDestination.Checked",
      request->destination);

  if (safe_browsing_pending_remote_.is_valid()) {
    // Bind the pipe created in DetachFromCurrentSequence to the current
    // sequence.
    safe_browsing_remote_.Bind(std::move(safe_browsing_pending_remote_));
    safe_browsing_ = safe_browsing_remote_.get();
  }

  original_url_ = request->url;
  pending_checks_++;
  start_request_time_ = base::TimeTicks::Now();
  is_start_request_called_ = true;
  // Use a weak pointer to self because |safe_browsing_| may not be owned by
  // this object.
  safe_browsing_->CreateCheckerAndCheck(
      frame_token_, url_checker_.BindNewPipeAndPassReceiver(), request->url,
      request->method, request->headers, request->load_flags,
      request->destination, request->has_user_gesture,
      request->originated_from_service_worker,
      base::BindOnce(&RendererURLLoaderThrottle::OnCheckUrlResult,
                     weak_factory_.GetWeakPtr()));
  safe_browsing_ = nullptr;

  url_checker_.set_disconnect_handler(base::BindOnce(
      &RendererURLLoaderThrottle::OnMojoDisconnect, base::Unretained(this)));
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
  is_response_from_cache_ =
      response_head->was_fetched_via_cache && !response_head->network_accessed;
  if (is_start_request_called_) {
    base::TimeTicks process_time = base::TimeTicks::Now();
    base::UmaHistogramTimes(
        "SafeBrowsing.RendererThrottle.IntervalBetweenStartAndProcess",
        process_time - start_request_time_);
    base::UmaHistogramTimes(
        base::StrCat(
            {"SafeBrowsing.RendererThrottle.IntervalBetweenStartAndProcess",
             is_response_from_cache_ ? kFromCacheUmaSuffix
                                     : kFromNetworkUmaSuffix}),
        process_time - start_request_time_);
    if (check_completed) {
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            base::TimeDelta());
    }
    is_start_request_called_ = false;
  }

  if (check_completed) {
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this), "original_url",
                                    original_url_.spec());
}

const char* RendererURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingRendererThrottle";
}

void RendererURLLoaderThrottle::OnCompleteCheck(bool proceed,
                                                bool showed_interstitial) {
  OnCompleteCheckInternal(true /* slow_check */, proceed, showed_interstitial);
}

void RendererURLLoaderThrottle::OnCheckUrlResult(
    mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
    bool proceed,
    bool showed_interstitial) {
  // When this is the callback of safe_browsing_->CreateCheckerAndCheck(), it is
  // possible that we get here after a check with |url_checker_| has completed
  // and blocked the request.
  if (blocked_ || !url_checker_)
    return;

  if (!slow_check_notifier.is_valid()) {
    OnCompleteCheckInternal(false /* slow_check */, proceed,
                            showed_interstitial);
    return;
  }

  pending_slow_checks_++;
  // Pending slow checks indicate that the resource may be unsafe. In that case,
  // pause reading response body from network to minimize the chance of
  // processing unsafe contents (e.g., writing unsafe contents into cache),
  // until we get the results. According to the results, we may resume reading
  // or cancel the resource load.
  if (pending_slow_checks_ == 1)
    delegate_->PauseReadingBodyFromNet();

  if (!notifier_receivers_) {
    notifier_receivers_ =
        std::make_unique<mojo::ReceiverSet<mojom::UrlCheckNotifier>>();
  }
  notifier_receivers_->Add(this, std::move(slow_check_notifier));
}

void RendererURLLoaderThrottle::OnCompleteCheckInternal(
    bool slow_check,
    bool proceed,
    bool showed_interstitial) {
  DCHECK(!blocked_);
  DCHECK(url_checker_);

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (slow_check) {
    DCHECK_LT(0u, pending_slow_checks_);
    pending_slow_checks_--;
  }

  // If the resource load is going to finish (either being cancelled or
  // resumed), record the total delay.
  if (!proceed || pending_checks_ == 0) {
    // If the resource load is currently deferred, there is a delay.
    if (deferred_) {
      total_delay_ = base::TimeTicks::Now() - defer_start_time_;
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            total_delay_);
    }
    LogTotalDelay3Metrics(total_delay_);
  }

  if (proceed) {
    if (pending_slow_checks_ == 0 && slow_check)
      delegate_->ResumeReadingBodyFromNet();

    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      base::UmaHistogramTimes("SafeBrowsing.RendererThrottle.TotalDelay",
                              total_delay_);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    url_checker_.reset();
    notifier_receivers_.reset();
    pending_checks_ = 0;
    pending_slow_checks_ = 0;
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
  notifier_receivers_.reset();

  pending_checks_ = 0;

  if (pending_slow_checks_ > 0) {
    pending_slow_checks_ = 0;
    delegate_->ResumeReadingBodyFromNet();
  }

  if (deferred_) {
    total_delay_ = base::TimeTicks::Now() - defer_start_time_;

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
