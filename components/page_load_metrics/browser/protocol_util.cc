// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/protocol_util.h"

namespace page_load_metrics {

NetworkProtocol GetNetworkProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info) {
  if (connection_info == net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1) {
    return NetworkProtocol::kHttp11;
  }
  if (connection_info == net::HttpResponseInfo::CONNECTION_INFO_HTTP2) {
    return NetworkProtocol::kHttp2;
  }
  if (net::HttpResponseInfo::ConnectionInfoToCoarse(connection_info) ==
      net::HttpResponseInfo::CONNECTION_INFO_COARSE_QUIC) {
    return NetworkProtocol::kQuic;
  }
  return NetworkProtocol::kOther;
}

}  // namespace page_load_metrics
