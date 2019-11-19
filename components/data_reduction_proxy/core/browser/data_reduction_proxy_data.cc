// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"

#include "base/memory/ptr_util.h"

namespace data_reduction_proxy {

const void* const kDataReductionProxyUserDataKey =
    &kDataReductionProxyUserDataKey;

DataReductionProxyData::DataReductionProxyData()
    : used_data_reduction_proxy_(false),
      lite_page_received_(false),
      black_listed_(false),
      was_cached_data_reduction_proxy_response_(false),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      connection_type_(net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

DataReductionProxyData::~DataReductionProxyData() {}

std::unique_ptr<DataReductionProxyData> DataReductionProxyData::DeepCopy()
    const {
  return std::make_unique<DataReductionProxyData>(*this);
}

DataReductionProxyData::DataReductionProxyData(
    const DataReductionProxyData& other) = default;

}  // namespace data_reduction_proxy
