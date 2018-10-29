// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_ascriber.h"

#include <memory>
#include <utility>

#include "components/data_use_measurement/core/data_use_recorder.h"
#include "net/http/http_response_headers.h"

namespace data_use_measurement {

DataUseAscriber::DataUseAscriber() {}
DataUseAscriber::~DataUseAscriber() {}

void DataUseAscriber::OnBeforeUrlRequest(net::URLRequest* request) {
  DataUseRecorder* recorder = GetOrCreateDataUseRecorder(request);
  if (!recorder)
    return;

  recorder->OnBeforeUrlRequest(request);
}

void DataUseAscriber::OnNetworkBytesSent(net::URLRequest* request,
                                         int64_t bytes_sent) {
  DataUseRecorder* recorder = GetDataUseRecorder(*request);
  if (!recorder)
    return;

  recorder->OnNetworkBytesSent(request, bytes_sent);
  for (auto& observer : observers_)
    observer.OnNetworkBytesUpdate(*request, &recorder->data_use());
}

void DataUseAscriber::OnNetworkBytesReceived(net::URLRequest* request,
                                             int64_t bytes_received) {
  DataUseRecorder* recorder = GetDataUseRecorder(*request);
  if (!recorder)
    return;

  recorder->OnNetworkBytesReceived(request, bytes_received);
  for (auto& observer : observers_)
    observer.OnNetworkBytesUpdate(*request, &recorder->data_use());
}

void DataUseAscriber::OnUrlRequestCompleted(net::URLRequest* request,
                                            bool started) {}

void DataUseAscriber::OnUrlRequestDestroyed(net::URLRequest* request) {
  DataUseRecorder* recorder = GetDataUseRecorder(*request);
  if (!recorder)
    return;

  recorder->OnUrlRequestDestroyed(request);
}

void DataUseAscriber::DisableAscriber() {}

}  // namespace data_use_measurement
