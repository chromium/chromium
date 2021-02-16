// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_

#include <string>

#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace web_package {

network::mojom::URLResponseHeadPtr CreateResourceResponse(
    const web_package::mojom::BundleResponsePtr& response);

std::string CreateHeaderString(
    const web_package::mojom::BundleResponsePtr& response);

network::mojom::URLResponseHeadPtr CreateResourceResponseFromHeaderString(
    const std::string& header_string);

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
