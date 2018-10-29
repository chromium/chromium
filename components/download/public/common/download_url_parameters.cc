// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_url_parameters.h"

namespace download {

DownloadUrlParameters::DownloadUrlParameters(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : DownloadUrlParameters(url,
                            -1,
                            -1,
                            -1,
                            traffic_annotation) {}

DownloadUrlParameters::DownloadUrlParameters(
    const GURL& url,
    int render_process_host_id,
    int render_view_host_routing_id,
    int render_frame_host_routing_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : content_initiated_(false),
      use_if_range_(true),
      method_("GET"),
      post_id_(-1),
      prefer_cache_(false),
      referrer_policy_(
          net::URLRequest::
              CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      render_process_host_id_(render_process_host_id),
      render_view_host_routing_id_(render_view_host_routing_id),
      render_frame_host_routing_id_(render_frame_host_routing_id),
      url_(url),
      do_not_prompt_for_login_(false),
      follow_cross_origin_redirects_(true),
      fetch_error_body_(false),
      transient_(false),
      traffic_annotation_(traffic_annotation),
      download_source_(DownloadSource::UNKNOWN) {}

DownloadUrlParameters::~DownloadUrlParameters() = default;

}  // namespace download
