// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_network_delegate.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"

namespace data_use_measurement {
DataUseNetworkDelegate::DataUseNetworkDelegate(
    std::unique_ptr<NetworkDelegate> nested_network_delegate,
    DataUseAscriber* ascriber,
    std::unique_ptr<DataUseMeasurement> data_use_measurement)
    : net::LayeredNetworkDelegate(std::move(nested_network_delegate)),
      ascriber_(ascriber),
      data_use_measurement_(std::move(data_use_measurement)) {
  DCHECK(ascriber);
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
}

DataUseNetworkDelegate::~DataUseNetworkDelegate() {}

void DataUseNetworkDelegate::OnBeforeURLRequestInternal(
    net::URLRequest* request,
    GURL* new_url) {
  ascriber_->OnBeforeUrlRequest(request);
  data_use_measurement_->OnBeforeURLRequest(request);
}

void DataUseNetworkDelegate::OnBeforeRedirectInternal(
    net::URLRequest* request,
    const GURL& new_location) {
  data_use_measurement_->OnBeforeRedirect(*request, new_location);
}

void DataUseNetworkDelegate::OnHeadersReceivedInternal(
    net::URLRequest* request,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  data_use_measurement_->OnHeadersReceived(request, original_response_headers);
}

void DataUseNetworkDelegate::OnNetworkBytesReceivedInternal(
    net::URLRequest* request,
    int64_t bytes_received) {
  ascriber_->OnNetworkBytesReceived(request, bytes_received);
  data_use_measurement_->OnNetworkBytesReceived(*request, bytes_received);
}

void DataUseNetworkDelegate::OnNetworkBytesSentInternal(
    net::URLRequest* request,
    int64_t bytes_sent) {
  ascriber_->OnNetworkBytesSent(request, bytes_sent);
  data_use_measurement_->OnNetworkBytesSent(*request, bytes_sent);
}

void DataUseNetworkDelegate::OnCompletedInternal(net::URLRequest* request,
                                                 bool started,
                                                 int net_error) {
  ascriber_->OnUrlRequestCompleted(request, started);
  data_use_measurement_->OnCompleted(*request, started);
}

void DataUseNetworkDelegate::OnURLRequestDestroyedInternal(
    net::URLRequest* request) {
  ascriber_->OnUrlRequestDestroyed(request);
}

}  // namespace data_use_measurement
