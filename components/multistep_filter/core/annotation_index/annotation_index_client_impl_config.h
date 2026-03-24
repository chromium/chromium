// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_CONFIG_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_CONFIG_H_

#include <stddef.h>

#include <string_view>

#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

// Configuration constants for the `AnnotationIndexClientImpl`.
namespace annotation_index_client_impl_config {

// The MIME type used when uploading Protocol Buffer data.
inline constexpr std::string_view kApplicationProtobufContentType =
    "application/x-protobuf";

// The maximum allowed download size for API responses (1 MB).
inline constexpr size_t kMaxDownloadSize = 1024 * 1024;

// The timeout duration for network requests.
inline constexpr base::TimeDelta kNetworkRequestTimeout = base::Seconds(10);

// This allows reading the error message within the API response when status
// is not 200 (e.g., 400). Otherwise, URL loader will not give any content in
// the response when there is a failure, which makes debugging hard.
inline constexpr bool kAllowHttpErrorResults = true;

}  // namespace annotation_index_client_impl_config

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_CONFIG_H_
