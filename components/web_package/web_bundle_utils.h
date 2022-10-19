// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_

#include <string>

#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace web_package {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScriptWebBundleOriginType {
  kSameOrigin = 0,
  kCrossOrigin = 1,
  kMaxValue = kCrossOrigin,
};

// The max memory limit per process of subrsource web bundles.
//
// Note: Currently the network service keeps the binary of the subresource web
// bundle in the memory. To protect the network service from OOM attacks, we
// set the max memory limit per renderer process. When the memory usage of
// subresource web bundles exceeds the limit, the web bundle loading fails,
// and the subresouce loading from the web bundle will fail on the page.
constexpr uint64_t kDefaultMaxMemoryPerProcess = 10ull * 1024 * 1024;

network::mojom::URLResponseHeadPtr CreateResourceResponse(
    const web_package::mojom::BundleResponsePtr& response);

std::string CreateHeaderString(
    const web_package::mojom::BundleResponsePtr& response);

network::mojom::URLResponseHeadPtr CreateResourceResponseFromHeaderString(
    const std::string& header_string);

// Returns true if |response| has "X-Content-Type-Options: nosniff" header.
bool HasNoSniffHeader(const network::mojom::URLResponseHead& response);

// Returns true if |url| is a valid uuid-in-package URL.
bool IsValidUuidInPackageURL(const GURL& url);

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
