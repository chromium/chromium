// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/prerender_url_loader_throttle.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/common/prerender_util.h"
#include "content/public/common/content_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace prerender {

namespace {

const char kPurposeHeaderName[] = "Purpose";
const char kPurposeHeaderValue[] = "prefetch";

void CancelPrerenderForUnsupportedMethod(
    PrerenderURLLoaderThrottle::CancelerGetterCallback callback) {
  chrome::mojom::PrerenderCanceler* canceler = std::move(callback).Run();
  if (canceler)
    canceler->CancelPrerenderForUnsupportedMethod();
}

void CancelPrerenderForUnsupportedScheme(
    PrerenderURLLoaderThrottle::CancelerGetterCallback callback,
    const GURL& url) {
  chrome::mojom::PrerenderCanceler* canceler = std::move(callback).Run();
  if (canceler)
    canceler->CancelPrerenderForUnsupportedScheme(url);
}

void CancelPrerenderForSyncDeferredRedirect(
    PrerenderURLLoaderThrottle::CancelerGetterCallback callback) {
  chrome::mojom::PrerenderCanceler* canceler = std::move(callback).Run();
  if (canceler)
    canceler->CancelPrerenderForSyncDeferredRedirect();
}

// Returns true if the response has a "no-store" cache control header.
bool IsNoStoreResponse(const network::mojom::URLResponseHead& response_head) {
  return response_head.headers &&
         response_head.headers->HasHeaderValue("cache-control", "no-store");
}

}  // namespace

PrerenderURLLoaderThrottle::PrerenderURLLoaderThrottle(
    PrerenderMode mode,
    const std::string& histogram_prefix,
    CancelerGetterCallback canceler_getter,
    scoped_refptr<base::SequencedTaskRunner> canceler_getter_task_runner)
    : mode_(mode),
      histogram_prefix_(histogram_prefix),
      canceler_getter_(std::move(canceler_getter)),
      canceler_getter_task_runner_(canceler_getter_task_runner) {
}

PrerenderURLLoaderThrottle::~PrerenderURLLoaderThrottle() {
  if (destruction_closure_)
    std::move(destruction_closure_).Run();
}

void PrerenderURLLoaderThrottle::PrerenderUsed() {
  if (original_request_priority_)
    delegate_->SetPriority(original_request_priority_.value());
  if (deferred_)
    delegate_->Resume();
}

void PrerenderURLLoaderThrottle::DetachFromCurrentSequence() {
  // This method is only called for synchronous XHR from the main thread.
  sync_xhr_ = true;
}

void PrerenderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (mode_ == PREFETCH_ONLY) {
    request->load_flags |= net::LOAD_PREFETCH;
    request->cors_exempt_headers.SetHeader(kPurposeHeaderName,
                                           kPurposeHeaderValue);
  }

  resource_type_ = static_cast<content::ResourceType>(request->resource_type);
  // Abort any prerenders that spawn requests that use unsupported HTTP
  // methods or schemes.
  if (!IsValidHttpMethod(mode_, request->method)) {
    // If this is a full prerender, cancel the prerender in response to
    // invalid requests.  For prefetches, cancel invalid requests but keep the
    // prefetch going.
    delegate_->CancelWithError(net::ERR_ABORTED);
    if (mode_ == DEPRECATED_FULL_PRERENDER) {
      canceler_getter_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(CancelPrerenderForUnsupportedMethod,
                                    std::move(canceler_getter_)));
      return;
    }
  }

  if (request->resource_type !=
          static_cast<int>(content::ResourceType::kMainFrame) &&
      !DoesSubresourceURLHaveValidScheme(request->url)) {
    // Destroying the prerender for unsupported scheme only for non-main
    // resource to allow chrome://crash to actually crash in the
    // *RendererCrash tests instead of being intercepted here. The
    // unsupported scheme for the main resource is checked in
    // WillRedirectRequest() and PrerenderContents::CheckURL(). See
    // http://crbug.com/673771.
    delegate_->CancelWithError(net::ERR_ABORTED);
    canceler_getter_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(CancelPrerenderForUnsupportedScheme,
                                  std::move(canceler_getter_), request->url));
    return;
  }

#if defined(OS_ANDROID)
  if (request->resource_type ==
      static_cast<int>(content::ResourceType::kFavicon)) {
    // Delay icon fetching until the contents are getting swapped in
    // to conserve network usage in mobile devices.
    *defer = true;
    return;
  }
#else
  // Priorities for prerendering requests are lowered, to avoid competing with
  // other page loads, except on Android where this is less likely to be a
  // problem. In some cases, this may negatively impact the performance of
  // prerendering, see https://crbug.com/652746 for details.
  // Requests with the IGNORE_LIMITS flag set (i.e., sync XHRs)
  // should remain at MAXIMUM_PRIORITY.
  if (request->load_flags & net::LOAD_IGNORE_LIMITS) {
    DCHECK_EQ(request->priority, net::MAXIMUM_PRIORITY);
  } else if (request->priority != net::IDLE) {
    original_request_priority_ = request->priority;
    request->priority = net::IDLE;
  }
#endif  // OS_ANDROID

  if (mode_ == PREFETCH_ONLY) {
    detached_timer_.Start(FROM_HERE,
                          base::TimeDelta::FromMilliseconds(
                              content::kDefaultDetachableCancelDelayMs),
                          this, &PrerenderURLLoaderThrottle::OnTimedOut);
  }
}

void PrerenderURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */) {
  redirect_count_++;
  if (mode_ == PREFETCH_ONLY) {
    RecordPrefetchResponseReceived(
        histogram_prefix_, content::IsResourceTypeFrame(resource_type_),
        true /* is_redirect */, IsNoStoreResponse(response_head));
  }

  std::string follow_only_when_prerender_shown_header;
  if (response_head.headers) {
    response_head.headers->GetNormalizedHeader(
        kFollowOnlyWhenPrerenderShown,
        &follow_only_when_prerender_shown_header);
  }
  // Abort any prerenders with requests which redirect to invalid schemes.
  if (!DoesURLHaveValidScheme(redirect_info->new_url)) {
    delegate_->CancelWithError(net::ERR_ABORTED);
    canceler_getter_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(CancelPrerenderForUnsupportedScheme,
                       std::move(canceler_getter_), redirect_info->new_url));
  } else if (follow_only_when_prerender_shown_header == "1" &&
             resource_type_ != content::ResourceType::kMainFrame) {
    // Only defer redirects with the Follow-Only-When-Prerender-Shown
    // header. Do not defer redirects on main frame loads.
    if (sync_xhr_) {
      // Cancel on deferred synchronous requests. Those will
      // indefinitely hang up a renderer process.
      canceler_getter_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(CancelPrerenderForSyncDeferredRedirect,
                                    std::move(canceler_getter_)));
      delegate_->CancelWithError(net::ERR_ABORTED);
    } else {
      // Defer the redirect until the prerender is used or canceled.
      *defer = true;
      deferred_ = true;
    }
  }
}

void PrerenderURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (mode_ != PREFETCH_ONLY)
    return;

  bool is_main_resource = content::IsResourceTypeFrame(resource_type_);
  RecordPrefetchResponseReceived(histogram_prefix_, is_main_resource,
                                 true /* is_redirect */,
                                 IsNoStoreResponse(*response_head));
  RecordPrefetchRedirectCount(histogram_prefix_, is_main_resource,
                              redirect_count_);
}

void PrerenderURLLoaderThrottle::OnTimedOut() {
  delegate_->CancelWithError(net::ERR_ABORTED);
}

}  // namespace prerender
