// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/util/type_safety/id_type.h"
#include "base/version.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

namespace feed {

// Information about the Chrome build.
struct ChromeInfo {
  version_info::Channel channel{};
  base::Version version;
};
// Device display metrics.
struct DisplayMetrics {
  float density;
  uint32_t width_pixels;
  uint32_t height_pixels;
};

// A unique ID for an ephemeral change.
using EphemeralChangeId = util::IdTypeU32<class EphemeralChangeIdClass>;
using SurfaceId = util::IdTypeU32<class SurfaceIdClass>;
using ImageFetchId = util::IdTypeU32<class ImageFetchIdClass>;

struct NetworkResponseInfo {
  NetworkResponseInfo();
  ~NetworkResponseInfo();
  NetworkResponseInfo(const NetworkResponseInfo&);
  NetworkResponseInfo& operator=(const NetworkResponseInfo&);

  // A union of net::Error (if the request failed) and the http
  // status code(if the request succeeded in reaching the server).
  int32_t status_code = 0;
  base::TimeDelta fetch_duration;
  base::Time fetch_time;
  std::string bless_nonce;
  GURL base_request_url;
  size_t response_body_bytes = 0;
};

struct NetworkResponse {
  // HTTP response body.
  std::string response_bytes;
  // HTTP status code if available, or net::Error otherwise.
  int status_code;

  NetworkResponse() = default;
  NetworkResponse(NetworkResponse&& other) = default;
  NetworkResponse& operator=(NetworkResponse&& other) = default;
};

// For the snippets-internals page.
struct DebugStreamData {
  static const int kVersion = 1;  // If a field changes, increment.

  DebugStreamData();
  ~DebugStreamData();
  DebugStreamData(const DebugStreamData&);
  DebugStreamData& operator=(const DebugStreamData&);

  base::Optional<NetworkResponseInfo> fetch_info;
  base::Optional<NetworkResponseInfo> upload_info;
  std::string load_stream_status;
};

std::string SerializeDebugStreamData(const DebugStreamData& data);
base::Optional<DebugStreamData> DeserializeDebugStreamData(
    base::StringPiece base64_encoded);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TYPES_H_
