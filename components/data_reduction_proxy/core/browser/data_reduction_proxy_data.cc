// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"

#include "base/memory/ptr_util.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

const void* const kDataReductionProxyUserDataKey =
    &kDataReductionProxyUserDataKey;

DataReductionProxyData::RequestInfo::RequestInfo(Protocol protocol,
                                                 bool proxy_bypass,
                                                 base::TimeDelta dns_time,
                                                 base::TimeDelta connect_time,
                                                 base::TimeDelta http_time)
    : protocol(protocol),
      proxy_bypass(proxy_bypass),
      dns_time(dns_time),
      connect_time(connect_time),
      http_time(http_time) {}

DataReductionProxyData::RequestInfo::RequestInfo(const RequestInfo& other)
    : protocol(other.protocol),
      proxy_bypass(other.proxy_bypass),
      dns_time(other.dns_time),
      connect_time(other.connect_time),
      http_time(other.http_time) {}

DataReductionProxyData::DataReductionProxyData()
    : used_data_reduction_proxy_(false),
      client_lofi_requested_(false),
      lite_page_received_(false),
      lofi_policy_received_(false),
      lofi_received_(false),
      black_listed_(false),
      was_cached_data_reduction_proxy_response_(false),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      connection_type_(net::NetworkChangeNotifier::CONNECTION_UNKNOWN),
      request_info_(std::vector<DataReductionProxyData::RequestInfo>()) {}

DataReductionProxyData::~DataReductionProxyData() {}

std::unique_ptr<DataReductionProxyData> DataReductionProxyData::DeepCopy()
    const {
  return std::make_unique<DataReductionProxyData>(*this);
}

DataReductionProxyData::DataReductionProxyData(
    const DataReductionProxyData& other) = default;

DataReductionProxyData* DataReductionProxyData::GetData(
    const net::URLRequest& request) {
  DataReductionProxyData* data = static_cast<DataReductionProxyData*>(
      request.GetUserData(kDataReductionProxyUserDataKey));
  return data;
}

DataReductionProxyData* DataReductionProxyData::GetDataAndCreateIfNecessary(
    net::URLRequest* request) {
  if (!request)
    return nullptr;
  DataReductionProxyData* data = GetData(*request);
  if (data)
    return data;
  data = new DataReductionProxyData();
  request->SetUserData(kDataReductionProxyUserDataKey, base::WrapUnique(data));
  return data;
}

void DataReductionProxyData::ClearData(net::URLRequest* request) {
  request->RemoveUserData(kDataReductionProxyUserDataKey);
}

std::vector<DataReductionProxyData::RequestInfo>
DataReductionProxyData::TakeRequestInfo() {
  return std::move(request_info_);
}

std::unique_ptr<DataReductionProxyData::RequestInfo>
DataReductionProxyData::CreateRequestInfoFromRequest(net::URLRequest* request,
                                                     bool did_bypass_proxy) {
  DCHECK(request);

  auto timing_info = std::make_unique<net::LoadTimingInfo>();
  request->GetLoadTimingInfo(timing_info.get());
  if (timing_info) {
    base::TimeDelta dns_time = timing_info->connect_timing.dns_end -
                               timing_info->connect_timing.dns_start;
    base::TimeDelta connect_time = timing_info->connect_timing.connect_end -
                                   timing_info->connect_timing.connect_start;
    base::TimeDelta http_time =
        timing_info->receive_headers_end - timing_info->send_start;
    DataReductionProxyData::RequestInfo::Protocol protocol;
    switch (request->proxy_server().scheme()) {
      case net::ProxyServer::SCHEME_HTTP:
        protocol = DataReductionProxyData::RequestInfo::Protocol::HTTP;
        break;
      case net::ProxyServer::SCHEME_HTTPS:
        protocol = DataReductionProxyData::RequestInfo::Protocol::HTTPS;
        break;
      case net::ProxyServer::SCHEME_QUIC:
        protocol = DataReductionProxyData::RequestInfo::Protocol::QUIC;
        break;
      default:
        protocol = DataReductionProxyData::RequestInfo::Protocol::UNKNOWN;
        break;
    }
    return std::make_unique<DataReductionProxyData::RequestInfo>(
        protocol, did_bypass_proxy, dns_time, connect_time, http_time);
  }
  return nullptr;
}

}  // namespace data_reduction_proxy
