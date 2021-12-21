// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/metrics_recorder_grpc.h"

#include "base/check.h"

namespace chromecast {

MetricsRecorderGrpc::MetricsRecorderGrpc() = default;

MetricsRecorderGrpc::~MetricsRecorderGrpc() = default;

void MetricsRecorderGrpc::SetClient(Client* client) {
  DCHECK((!!metrics_recorder_client_) ^ (!!client));
  metrics_recorder_client_ = client;
}

}  // namespace chromecast
