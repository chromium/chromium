// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_REQUEST_H_
#define COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/backoff_entry.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace sync_pb {
class ExperimentStatusResponse;
}

namespace gcm {

// Defines the request to talk with the server to determine if the GCM support
// should be enabled.
class GCMChannelStatusRequest {
 public:
  // Callback completing the channel status request.
  // |update_received|: use the existing values if it is false which means no
  //                    update is received.
  // |enabled|: indicates if GCM is enabled (allowed to run) or not.
  // |poll_interval_seconds|: the interval in seconds to start next poll
  //                          request.
  typedef base::Callback<void(bool update_received,
                              bool enabled,
                              int poll_interval_seconds)>
      GCMChannelStatusRequestCallback;

  GCMChannelStatusRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& channel_status_request_url,
      const std::string& user_agent,
      const GCMChannelStatusRequestCallback& callback);
  ~GCMChannelStatusRequest();

  void Start();

  static int default_poll_interval_seconds();
  static int min_poll_interval_seconds();

  // Public so tests can use it.
  void ParseResponseProto(sync_pb::ExperimentStatusResponse response_proto);

 private:
  FRIEND_TEST_ALL_PREFIXES(GCMChannelStatusRequestTest, RequestData);

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  bool ParseResponse(std::unique_ptr<std::string> response_body);
  void RetryWithBackoff(bool update_backoff);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string channel_status_request_url_;
  const std::string user_agent_;
  GCMChannelStatusRequestCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  net::BackoffEntry backoff_entry_;
  base::WeakPtrFactory<GCMChannelStatusRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GCMChannelStatusRequest);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_REQUEST_H_
