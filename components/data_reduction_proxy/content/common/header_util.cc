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

  if (resource_type == content::ResourceType::kMedia) {
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
      resource_type == content::ResourceType::kMainFrame) {
    accept_transform_value = lite_page_directive();
  }

  if (accept_transform_value.empty())
    return;

  headers->SetHeader(chrome_proxy_accept_transform_header(),
                     accept_transform_value);
}

}  // namespace data_reduction_proxy
