// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_DUMMY_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_DUMMY_H_

#include "chromecast/external_mojo/external_service_support/tracing_client.h"

namespace chromecast {
namespace external_service_support {

// Dummy implementation for build configurations that do not enable tracing.
class TracingClientDummy : public TracingClient {
 public:
  TracingClientDummy() = default;
  ~TracingClientDummy() override = default;

 private:
  TracingClientDummy(const TracingClientDummy&) = delete;
  TracingClientDummy& operator=(const TracingClientDummy&) = delete;
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_DUMMY_H_
