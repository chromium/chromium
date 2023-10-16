// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/common/prerender_url_loader_throttle.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"
#include "content/public/common/content_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"

namespace prerender {

namespace {

const char kPurposeHeaderName[] = "Purpose";
const char kPurposeHeaderValue[] = "prefetch";

void CallCancelPrerenderForUnsupportedScheme(
    mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler) {
  mojo::Remote<prerender::mojom::PrerenderCanceler>(std::move(canceler))
      ->CancelPrerenderForUnsupportedScheme();
}

// Returns true if the response has a "no-store" cache control header.
bool IsNoStoreResponse(const network::mojom::URLResponseHead& response_head) {
  return response_head.headers &&
         response_head.headers->HasHeaderValue("cache-control", "no-store");
}

}  // namespace

PrerenderURLLoaderThrottle::PrerenderURLLoaderThrottle(
    const std::string& histogram_prefix,
    mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler)
    : histogram_prefix_(histogram_prefix), canceler_(std::move(canceler)) {
  DCHECK(canceler_);
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
  if (destruction_closure_) {
    destruction_closure_ = base::BindOnce(
        [](scoped_refptr<base::SequencedTaskRunner> task_runner,
           base::OnceClosure destruction_closure) {
          task_runner->PostTask(FROM_HERE, std::move(destruction_closure));
        },
        base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(destruction_closure_));
  }
}

void PrerenderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request->load_flags |= net::LOAD_PREFETCH;
  request->cors_exempt_headers.SetHeader(kPurposeHeaderName,
                                         kPurposeHeaderValue);

  request_destination_ = request->destination;
  // Abort any prerenders that spawn requests that use unsupported HTTP
  // methods or schemes.
  if (!IsValidHttpMethod(request->method)) {
    // If this is a full prerender, cancel the prerender in response to
    // invalid requests.  For prefetches, cancel invalid requests but keep the
    // prefetch going.
    delegate_->CancelWithError(net::ERR_ABORTED);
  }

  if (request->destination != network::mojom::RequestDestination::kDocument &&
      !DoesSubresourceURLHaveValidScheme(request->url)) {
    // Destroying the prerender for unsupported scheme only for non-main
    // resource to allow chrome://crash to actually crash in the
    // *RendererCrash tests instead of being intercepted here. The
    // unsupported scheme for the main resource is checked in
    // WillRedirectRequest() and NoStatePrefetchContents::CheckURL(). See
    // http://crbug.com/673771.
    delegate_->CancelWithError(net::ERR_ABORTED);
    CallCancelPrerenderForUnsupportedScheme(std::move(canceler_));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (request->is_favicon) {
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
#endif  // BUILDFLAG(IS_ANDROID)

  detached_timer_.Start(
      FROM_HERE, base::Milliseconds(content::kDefaultDetachableCancelDelayMs),
      this, &PrerenderURLLoaderThrottle::OnTimedOut);
}

const char* PrerenderURLLoaderThrottle::NameForLoggingWillStartRequest() {
  return "PrerenderThrottle";
}

void PrerenderURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  redirect_count_++;
  RecordPrefetchResponseReceived(
      histogram_prefix_, blink::IsRequestDestinationFrame(request_destination_),
      true /* is_redirect */, IsNoStoreResponse(response_head));

  std::string follow_only_when_prerender_shown_header;
  if (response_head.headers) {
    response_head.headers->GetNormalizedHeader(
        kFollowOnlyWhenPrerenderShown,
        &follow_only_when_prerender_shown_header);
  }
  // Abort any prerenders with requests which redirect to invalid schemes.
  if (!DoesURLHaveValidScheme(redirect_info->new_url)) {
    delegate_->CancelWithError(net::ERR_ABORTED);
    CallCancelPrerenderForUnsupportedScheme(std::move(canceler_));
  } else if (follow_only_when_prerender_shown_header == "1" &&
             request_destination_ !=
                 network::mojom::RequestDestination::kDocument) {
    // Only defer redirects with the Follow-Only-When-Prerender-Shown
    // header. Do not defer redirects on main frame loads.
    *defer = true;
    deferred_ = true;
  }
}

void PrerenderURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  bool is_main_resource =
      blink::IsRequestDestinationFrame(request_destination_);
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
