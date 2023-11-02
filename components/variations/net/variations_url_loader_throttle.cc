// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_url_loader_throttle.h"

#include "components/google/core/common/google_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace variations {
namespace {

// Returns the Owner corresponding to |top_frame_origin|.
Owner GetOwner(const url::Origin& top_frame_origin) {
  // Use GetTupleOrPrecursorTupleIfOpaque().GetURL() rather than just GetURL()
  // to handle sandboxed top frames in addition to non-sandboxed ones.
  // top_frame_origin.GetURL() handles only the latter.
  const GURL url(top_frame_origin.GetTupleOrPrecursorTupleIfOpaque().GetURL());
  if (!url.is_valid())
    return Owner::kUnknownFromRenderer;
  return google_util::IsGoogleAssociatedDomainUrl(url) ? Owner::kGoogle
                                                       : Owner::kNotGoogle;
}

}  // namespace

VariationsURLLoaderThrottle::VariationsURLLoaderThrottle(
    variations::mojom::VariationsHeadersPtr variations_headers)
    : variations_headers_(std::move(variations_headers)),
      owner_(Owner::kUnknown) {}

VariationsURLLoaderThrottle::VariationsURLLoaderThrottle(
    variations::mojom::VariationsHeadersPtr variations_headers,
    const url::Origin& top_frame_origin)
    : variations_headers_(std::move(variations_headers)),
      owner_(GetOwner(top_frame_origin)) {}

VariationsURLLoaderThrottle::~VariationsURLLoaderThrottle() = default;

// static
void VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
    const variations::VariationsClient* variations_client,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  if (!variations_client || variations_client->IsOffTheRecord())
    return;

  throttles->push_back(std::make_unique<VariationsURLLoaderThrottle>(
      variations_client->GetVariationsHeaders()));
}

void VariationsURLLoaderThrottle::DetachFromCurrentSequence() {}

void VariationsURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (variations_headers_.is_null())
    return;

  // InIncognito::kNo is passed because this throttle is never created in
  // incognito mode.
  //
  // |variations_headers_| is moved rather than cloned because a
  // VariationsURLLoaderThrottle is created for each request and
  // WillStartRequest() is called only once—from ThrottlingURLLoader::Start().
  variations::AppendVariationsHeaderWithCustomValue(
      request->url, InIncognito::kNo, std::move(variations_headers_), owner_,
      request);
}

void VariationsURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  variations::RemoveVariationsHeaderIfNeeded(*redirect_info, response_head,
                                             to_be_removed_headers);
}

}  // namespace variations
