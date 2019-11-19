// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/renderer/renderer_url_loader_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/safe_browsing/common/utils.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"

namespace safe_browsing {

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : safe_browsing_(safe_browsing), render_frame_id_(render_frame_id) {}

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() {
  if (deferred_)
    TRACE_EVENT_ASYNC_END0("safe_browsing", "Deferred", this);
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

  if (safe_browsing_pending_remote_.is_valid()) {
    // Bind the pipe created in DetachFromCurrentSequence to the current
    // sequence.
    safe_browsing_remote_.Bind(std::move(safe_browsing_pending_remote_));
    safe_browsing_ = safe_browsing_remote_.get();
  }

  original_url_ = request->url;
  pending_checks_++;
  // Use a weak pointer to self because |safe_browsing_| may not be owned by
  // this object.
  net::HttpRequestHeaders headers;
  headers.CopyFrom(request->headers);
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, url_checker_.BindNewPipeAndPassReceiver(), request->url,
      request->method, headers, request->load_flags,
      static_cast<content::ResourceType>(request->resource_type),
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
    net::HttpRequestHeaders* /* modified_headers */) {
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

  if (pending_checks_ == 0)
    return;

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_ASYNC_BEGIN1("safe_browsing", "Deferred", this, "original_url",
                           original_url_.spec());
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

  user_action_involved_ = user_action_involved_ || showed_interstitial;
  // If the resource load is currently deferred and is going to exit that state
  // (either being cancelled or resumed), record the total delay.
  if (deferred_ && (!proceed || pending_checks_ == 0))
    total_delay_ = base::TimeTicks::Now() - defer_start_time_;

  if (proceed) {
    if (pending_slow_checks_ == 0 && slow_check)
      delegate_->ResumeReadingBodyFromNet();

    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_ASYNC_END0("safe_browsing", "Deferred", this);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    url_checker_.reset();
    notifier_receivers_.reset();
    pending_checks_ = 0;
    pending_slow_checks_ = 0;
    delegate_->CancelWithError(GetNetErrorCodeForSafeBrowsing(),
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
    TRACE_EVENT_ASYNC_END0("safe_browsing", "Deferred", this);
    delegate_->Resume();
  }
}

}  // namespace safe_browsing
