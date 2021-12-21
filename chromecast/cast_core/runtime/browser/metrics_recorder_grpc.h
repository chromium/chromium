// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_METRICS_RECORDER_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_METRICS_RECORDER_GRPC_H_

#include "base/callback.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"

namespace chromecast {

// This is an interface that allows asynchronous access to the Cast Core
// MetricsRecorderService API.
class MetricsRecorderGrpc {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Called after a prior Record() call is acknowledged by Cast Core and
    // another Record() call can begin.
    virtual void OnRecordComplete() = 0;

    // Called when metrics reporting should wind down.  |complete_callback|
    // should be called when no more calls to Record() will happen.
    virtual void OnCloseSoon(base::OnceClosure complete_callback) = 0;
  };

  MetricsRecorderGrpc();
  virtual ~MetricsRecorderGrpc() = 0;

  // Sends |request| to Cast Core.  Client::OnRecordComplete will be called when
  // this is acknowledged by Cast Core.
  virtual void Record(const cast::metrics::RecordRequest& request) = 0;

  void SetClient(Client* client);

 protected:
  Client* metrics_recorder_client_{nullptr};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_METRICS_RECORDER_GRPC_H_
