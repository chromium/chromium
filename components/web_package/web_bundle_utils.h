// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_

#include <string>

#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace web_package {

network::mojom::URLResponseHeadPtr CreateResourceResponse(
    const web_package::mojom::BundleResponsePtr& response);

std::string CreateHeaderString(
    const web_package::mojom::BundleResponsePtr& response);

network::mojom::URLResponseHeadPtr CreateResourceResponseFromHeaderString(
    const std::string& header_string);

// Returns true if |response| has "X-Content-Type-Options: nosniff" header.
bool HasNoSniffHeader(const network::mojom::URLResponseHead& response);

// Returns true if |url| is a valid UUID URN, i.e. |url| is a URN (RFC 2141)
// whose NID is "uuid" and its NSS confirms to the syntactic structure
// described in RFC 4122, section 3.
bool IsValidUrnUuidURL(const GURL& url);

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
