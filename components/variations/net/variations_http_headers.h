// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
struct NetworkTrafficAnnotationTag;
struct RedirectInfo;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

class GURL;

namespace variations {

enum class InIncognito { kNo, kYes };

enum class SignedIn { kNo, kYes };

// Adds Chrome experiment and metrics state as custom headers to |request|.
// The content of the headers will depend on |incognito| and |signed_in|
// parameters. It is fine to pass SignedIn::NO if the state is not known to the
// caller. This will prevent addition of ids of type
// GOOGLE_WEB_PROPERTIES_SIGNED_IN, which is not the case for any ids that come
// from the variations server. These headers are never transmitted to non-Google
// web sites, which is checked based on the destination |url|.
// Returns true if custom headers are added. Returns false otherwise.
bool AppendVariationsHeader(const GURL& url,
                            InIncognito incognito,
                            SignedIn signed_in,
                            network::ResourceRequest* request);

// Similar to AppendVariationsHeader, but uses specified |variations_header| as
// the custom header value. You should not generally need to use this.
bool AppendVariationsHeaderWithCustomValue(const GURL& url,
                                           InIncognito incognito,
                                           const std::string& variations_header,
                                           network::ResourceRequest* request);

// Adds Chrome experiment and metrics state as a custom header to |request|
// when the signed-in state is not known to the caller; See above for details.
bool AppendVariationsHeaderUnknownSignedIn(const GURL& url,
                                           InIncognito incognito,
                                           network::ResourceRequest* request);

// Removes the variations header for requests when a redirect to a non-Google
// URL occurs.
void RemoveVariationsHeaderIfNeeded(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers);

// Creates a SimpleURLLoader that will include the variations header for
// requests to Google and ensures they're removed if a redirect to a non-Google
// URL occurs.
std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeader(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    SignedIn signed_in,
    const net::NetworkTrafficAnnotationTag& annotation_tag);

// Creates a SimpleURLLoader that will include the variations header for
// requests to Google when the signed-in state is unknown and ensures they're
// removed if a redirect to a non-Google URL occurs.
std::unique_ptr<network::SimpleURLLoader>
CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
    std::unique_ptr<network::ResourceRequest> request,
    InIncognito incognito,
    const net::NetworkTrafficAnnotationTag& annotation_tag);

// Checks if |header_name| is one for the variations header.
bool IsVariationsHeader(const std::string& header_name);

// Checks if |request| contains the variations header.
bool HasVariationsHeader(const network::ResourceRequest& request);

// Calls the internal ShouldAppendVariationsHeader() for testing.
bool ShouldAppendVariationsHeaderForTesting(const GURL& url);

// Updates |cors_exempt_header_list| field of the given |param| to register the
// variation headers.
void UpdateCorsExemptHeaderForVariations(
    network::mojom::NetworkContextParams* params);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADERS_H_
