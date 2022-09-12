// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PERFETTO_TRACING_CLIENT_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PERFETTO_TRACING_CLIENT_H_

#include <memory>

#include "base/tracing/perfetto_platform.h"
#include "chromecast/external_mojo/external_service_support/tracing_client.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event.h"

namespace chromecast {
namespace external_service_support {
class ExternalConnector;

// TracingClient implementation for using Perfetto tracing.
class PerfettoTracingClient : public TracingClient {
 public:
  PerfettoTracingClient();

  ~PerfettoTracingClient() override;

  PerfettoTracingClient(const PerfettoTracingClient&) = delete;
  PerfettoTracingClient& operator=(const PerfettoTracingClient&) = delete;

 private:
  const std::unique_ptr<base::tracing::PerfettoPlatform> perfetto_platform_;
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PERFETTO_TRACING_CLIENT_H_
