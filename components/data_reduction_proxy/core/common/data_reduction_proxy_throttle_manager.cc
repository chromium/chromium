// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"

#include "base/memory/ptr_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

DataReductionProxyThrottleManager::DataReductionProxyThrottleManager(
    mojom::DataReductionProxy* data_reduction_proxy,
    mojom::DataReductionProxyThrottleConfigPtr initial_config)
    : shared_data_reduction_proxy_(data_reduction_proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  shared_data_reduction_proxy_->AddThrottleConfigObserver(
      receiver_.BindNewPipeAndPassRemote());

  OnThrottleConfigChanged(std::move(initial_config));
}

DataReductionProxyThrottleManager::~DataReductionProxyThrottleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (DataReductionProxyThrottleConfigCheckedObserver& observer :
       same_sequence_observers_) {
    observer.OnThrottleManagerDestroyed(this);
  }
}

void DataReductionProxyThrottleManager::OnThrottleConfigChanged(
    mojom::DataReductionProxyThrottleConfigPtr config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_proxy_config_ = config.Clone();

  for (DataReductionProxyThrottleConfigCheckedObserver& observer :
       same_sequence_observers_) {
    observer.OnThrottleConfigChanged(config.Clone());
  }
}

void DataReductionProxyThrottleManager::AddSameSequenceObserver(
    DataReductionProxyThrottleConfigCheckedObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  same_sequence_observers_.AddObserver(observer);
}

void DataReductionProxyThrottleManager::RemoveSameSequenceObserver(
    DataReductionProxyThrottleConfigCheckedObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  same_sequence_observers_.RemoveObserver(observer);
}

// static
mojom::DataReductionProxyThrottleConfigPtr
DataReductionProxyThrottleManager::CreateConfig(
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  auto config = mojom::DataReductionProxyThrottleConfig::New();

  for (const auto& drp_server : proxies_for_http) {
    auto converted = mojom::DataReductionProxyServer::New();
    converted->is_core = drp_server.IsCoreProxy();
    converted->proxy_server = drp_server.proxy_server();

    config->proxies_for_http.push_back(std::move(converted));
  }

  return config;
}

}  // namespace data_reduction_proxy
