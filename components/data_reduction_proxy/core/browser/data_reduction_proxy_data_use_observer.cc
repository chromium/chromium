// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data_use_observer.h"

#include <memory>
#include <string>

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "components/data_use_measurement/core/data_use.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

class DataUseUserDataBytes : public base::SupportsUserData::Data {
 public:
  // Key used to store data usage in userdata until the page URL is available.
  static const void* const kUserDataKey;

  DataUseUserDataBytes(int64_t network_bytes, int64_t original_bytes)
      : network_bytes_(network_bytes), original_bytes_(original_bytes) {}

  int64_t network_bytes() const { return network_bytes_; }
  int64_t original_bytes() const { return original_bytes_; }

  void IncrementBytes(int64_t network_bytes, int64_t original_bytes) {
    network_bytes_ += network_bytes;
    original_bytes_ += original_bytes;
  }

 private:
  int64_t network_bytes_;
  int64_t original_bytes_;
};

// static
const void* const DataUseUserDataBytes::kUserDataKey =
    &DataUseUserDataBytes::kUserDataKey;

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyDataUseObserver::DataReductionProxyDataUseObserver(
    DataReductionProxyIOData* data_reduction_proxy_io_data,
    data_use_measurement::DataUseAscriber* data_use_ascriber)
    : data_reduction_proxy_io_data_(data_reduction_proxy_io_data),
      data_use_ascriber_(data_use_ascriber) {
  DCHECK(data_reduction_proxy_io_data_);
  if (!data_reduction_proxy::params::IsDataSaverSiteBreakdownUsingPLMEnabled())
    data_use_ascriber_->AddObserver(this);
}

DataReductionProxyDataUseObserver::~DataReductionProxyDataUseObserver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  data_use_ascriber_->RemoveObserver(this);
}

void DataReductionProxyDataUseObserver::OnPageLoadCommit(
    data_use_measurement::DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!data_use->url().is_valid())
    return;
  DataUseUserDataBytes* bytes = reinterpret_cast<DataUseUserDataBytes*>(
      data_use->GetUserData(DataUseUserDataBytes::kUserDataKey));
  if (bytes) {
    // Record the data use bytes saved in user data to database.
    data_reduction_proxy_io_data_->UpdateDataUseForHost(
        bytes->network_bytes(), bytes->original_bytes(),
        data_use->url().HostNoBrackets());
    data_use->RemoveUserData(DataUseUserDataBytes::kUserDataKey);
  }
}

void DataReductionProxyDataUseObserver::OnPageResourceLoad(
    const net::URLRequest& request,
    data_use_measurement::DataUse* data_use) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!request.url().SchemeIs(url::kHttpsScheme) &&
      !request.url().SchemeIs(url::kHttpScheme)) {
    return;
  }

  if (request.GetTotalReceivedBytes() <= 0)
    return;

  int64_t network_bytes = request.GetTotalReceivedBytes();

  // Estimate how many bytes would have been used if the DataReductionProxy was
  // not used, and record the data usage.
  int64_t original_bytes = util::EstimateOriginalReceivedBytes(
      request, data_reduction_proxy_io_data_->lofi_decider());

  if (data_use->traffic_type() ==
          data_use_measurement::DataUse::TrafficType::USER_TRAFFIC &&
      !data_use->url().is_valid()) {
    // URL will be empty until pageload navigation commits. Save the data use of
    // these mainframe, subresource, redirected requests in user data until
    // then.
    DataUseUserDataBytes* bytes = reinterpret_cast<DataUseUserDataBytes*>(
        data_use->GetUserData(DataUseUserDataBytes::kUserDataKey));
    if (bytes) {
      bytes->IncrementBytes(network_bytes, original_bytes);
    } else {
      data_use->SetUserData(DataUseUserDataBytes::kUserDataKey,
                            std::make_unique<DataUseUserDataBytes>(
                                network_bytes, original_bytes));
    }
  } else {
    // Report the datause that cannot be scoped to a page load to the other
    // host. These include chrome services, service-worker, Downloads, etc.
    data_reduction_proxy_io_data_->UpdateDataUseForHost(
        network_bytes, original_bytes,
        data_use->traffic_type() ==
                data_use_measurement::DataUse::TrafficType::USER_TRAFFIC
            ? data_use->url().HostNoBrackets()
            : util::GetSiteBreakdownOtherHostName());
  }
}

void DataReductionProxyDataUseObserver::OnPageDidFinishLoad(
    data_use_measurement::DataUse* data_use) {
}

}  // namespace data_reduction_proxy
