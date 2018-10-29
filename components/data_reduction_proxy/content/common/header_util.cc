// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/header_util.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

void MaybeSetAcceptTransformHeader(const GURL& url,
                                   content::ResourceType resource_type,
                                   content::PreviewsState previews_state,
                                   net::HttpRequestHeaders* headers) {
  // Previews only operate on HTTP.
  if (!url.SchemeIs("http"))
    return;

  // Chrome-Proxy-Accept-Transform takes at most one token.
  if (headers->HasHeader(chrome_proxy_accept_transform_header()))
    return;

  if (resource_type == content::RESOURCE_TYPE_MEDIA) {
    headers->SetHeader(chrome_proxy_accept_transform_header(),
                       compressed_video_directive());
    return;
  }

  // Do not add the Chrome-Proxy-Accept-Transform header when the page load
  // explicitly forbids previews transformations.
  if (previews_state & content::PREVIEWS_NO_TRANSFORM ||
      previews_state & content::PREVIEWS_OFF) {
    return;
  }

  std::string accept_transform_value;
  if ((previews_state & content::SERVER_LITE_PAGE_ON) &&
      resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    accept_transform_value = lite_page_directive();
  } else if ((previews_state & content::SERVER_LOFI_ON)) {
    // Note that for subresource requests, the Lo-Fi bit should only be set
    // if the main frame response provided the "empty-image" directive (for
    // the client to echo back to the server here for any image resources).
    // Also, it should only be set for subresource requests that might be
    // image requests.
    bool resource_type_supports_empty_image =
        !(resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
          resource_type == content::RESOURCE_TYPE_STYLESHEET ||
          resource_type == content::RESOURCE_TYPE_SCRIPT ||
          resource_type == content::RESOURCE_TYPE_FONT_RESOURCE ||
          resource_type == content::RESOURCE_TYPE_MEDIA ||
          resource_type == content::RESOURCE_TYPE_CSP_REPORT);
    if (resource_type_supports_empty_image) {
      accept_transform_value = empty_image_directive();
    }
  }

  if (accept_transform_value.empty())
    return;

  headers->SetHeader(chrome_proxy_accept_transform_header(),
                     accept_transform_value);
}

}  // namespace data_reduction_proxy
