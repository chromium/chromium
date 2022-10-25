// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace safe_browsing {

namespace {

// Returns true if the URL is known to be safe. We also require that this URL
// never redirects to a potentially unsafe URL.
bool KnownSafeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : safe_browsing_(safe_browsing), render_frame_id_(render_frame_id) {}

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
}

void RendererURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

  if (KnownSafeUrl(request->url))
    return;

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
  net::HttpRequestHeaders headers;
  headers.CopyFrom(request->headers);
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, url_checker_.BindNewPipeAndPassReceiver(), request->url,
      request->method, headers, request->load_flags, request->destination,
      request->has_user_gesture, request->originated_from_service_worker,
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
  if (is_start_request_called_) {
    base::UmaHistogramTimes(
        "SafeBrowsing.RendererThrottle.IntervalBetweenStartAndProcess",
        base::TimeTicks::Now() - start_request_time_);
    is_start_request_called_ = false;
  }

  if (check_completed)
    return;

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

void RendererURLLoaderThrottle::OnCompleteCheck(
    bool proceed,
    bool showed_interstitial,
    bool did_perform_real_time_check,
    bool did_check_allowlist) {
  OnCompleteCheckInternal(true /* slow_check */, proceed, showed_interstitial);
}

void RendererURLLoaderThrottle::OnCheckUrlResult(
    mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
    bool proceed,
    bool showed_interstitial,
    bool did_perform_real_time_check,
    bool did_check_allowlist) {
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
    if (deferred_)
      total_delay_ = base::TimeTicks::Now() - defer_start_time_;
    base::UmaHistogramTimes("SafeBrowsing.RendererThrottle.TotalDelay2",
                            total_delay_);
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

}  // namespace safe_browsing
