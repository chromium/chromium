// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

#include "base/logging.h"

namespace data_reduction_proxy {

DataReductionProxyServer::DataReductionProxyServer(
    const net::ProxyServer& proxy_server)
    : proxy_server_(proxy_server) {}

bool DataReductionProxyServer::operator==(
    const DataReductionProxyServer& other) const {
  return proxy_server_ == other.proxy_server_;
}

// static
std::vector<net::ProxyServer>
DataReductionProxyServer::ConvertToNetProxyServers(
    const std::vector<DataReductionProxyServer>& data_reduction_proxy_servers) {
  std::vector<net::ProxyServer> net_proxy_servers;
  net_proxy_servers.reserve(data_reduction_proxy_servers.size());

  for (const auto& data_reduction_proxy_server : data_reduction_proxy_servers)
    net_proxy_servers.push_back(data_reduction_proxy_server.proxy_server());
  return net_proxy_servers;
}

bool DataReductionProxyServer::IsCoreProxy() const {
  return true;
}

bool DataReductionProxyServer::IsSecureProxy() const {
  return proxy_server_.is_https() || proxy_server_.is_quic();
}

}  // namespace data_reduction_proxy
