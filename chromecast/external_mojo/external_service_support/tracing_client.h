// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_H_

#include <memory>

namespace chromecast {
namespace external_service_support {
class ExternalConnector;

// TracingClient Supports the tracing of processes that connect to a central
// tracing service through an ExternalConnector.
class TracingClient {
 public:
  TracingClient() = default;
  virtual ~TracingClient() = default;

  static const char kTracingServiceName[];

  static std::unique_ptr<TracingClient> Create(ExternalConnector* connector);

 private:
  TracingClient(const TracingClient&) = delete;
  TracingClient& operator=(const TracingClient&) = delete;
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_TRACING_CLIENT_H_
