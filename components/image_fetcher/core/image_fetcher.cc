// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher.h"

namespace image_fetcher {

ImageFetcherParams::ImageFetcherParams(
    const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag,
    std::string uma_client_name)
    : network_traffic_annotation_tag_(network_traffic_annotation_tag),
      uma_client_name_(uma_client_name),
      skip_transcoding_(false),
      skip_disk_cache_read_(false),
      allow_needs_transcoding_file_(false) {}

ImageFetcherParams::ImageFetcherParams(const ImageFetcherParams& params) =
    default;

ImageFetcherParams::ImageFetcherParams(ImageFetcherParams&& params) = default;

ImageFetcherParams::~ImageFetcherParams() = default;

}  // namespace image_fetcher
