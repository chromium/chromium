// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_
#define COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_

#include <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/origin.h"

namespace base {
class TimeDelta;
}  // namespace base

// URLSession is Apple's API for performing URL requests. This namespace
// provides a few helper functions to convert between Chrome's network objects
// and Apple's native objects to ease the use of the aforementioned API.
namespace url_session_helper {

// This function only takes care of: URL, headers, method, body and timeout.
//
// Sets the Origin header with |request.request_initiator|, which must be set
// and verified beforehand.
//
// Returns nil if conversion of URL or method fails. Will ignore
// headers where conversion between std::string and NSString failed. Headers are
// allowlisted, see kRequestHeadersAllowlist in the .mm file for details.
//
// Only supports request body of type network::DataElementBytes, for other types
// body will be set to nil.
COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
NSURLRequest* ConvertResourceRequest(const network::ResourceRequest& request,
                                     base::TimeDelta timeout);

// Only converts: mime_type, content_length, network_accessed
// and HTTP headers.
//
// Filters headers by Access-Control-Allow-Headers.
//
// When converting HTTP headers will use hard-coded HTTP 1.1 for simplicity.
//
// Assumes |response| is not nil.
//
// Uses |request_initiator| to verify the Access-Control-Allow-Origin header.
// If the header is missing or does not match the |request_initiator| returns
// nullptr;
COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
network::mojom::URLResponseHeadPtr ConvertNSURLResponse(
    NSURLResponse* response,
    const url::Origin& request_initiator);

// Checks if request matches pattern of Okta's SSO URL request, which is:
// POST
// https://<DOMAIN>/idp/idx/authenticators/sso_extension/transactions/<ID>/verify
COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH)
bool IsOktaSSORequest(const network::ResourceRequest& request);

}  // namespace url_session_helper

#endif  // COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_
