// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PROTOCOL_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PROTOCOL_UTIL_H_

#include "net/http/http_connection_info.h"

namespace page_load_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NetworkProtocol { kHttp11, kHttp2, kQuic, kOther };

// Returns a higher-level enum summary of the protocol described by the
// ConnectionInfo enum.
NetworkProtocol GetNetworkProtocol(net::HttpConnectionInfo connection_info);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PROTOCOL_UTIL_H_
