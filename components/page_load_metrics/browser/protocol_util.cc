// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/protocol_util.h"

namespace page_load_metrics {

NetworkProtocol GetNetworkProtocol(net::HttpConnectionInfo connection_info) {
  if (connection_info == net::HttpConnectionInfo::kHTTP1_1) {
    return NetworkProtocol::kHttp11;
  }
  if (connection_info == net::HttpConnectionInfo::kHTTP2) {
    return NetworkProtocol::kHttp2;
  }
  if (net::HttpConnectionInfoToCoarse(connection_info) ==
      net::HttpConnectionInfoCoarse::kQUIC) {
    return NetworkProtocol::kQuic;
  }
  return NetworkProtocol::kOther;
}

}  // namespace page_load_metrics
