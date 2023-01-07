// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/external_mojo/external_service_support/perfetto_tracing_client.h"

namespace chromecast {
namespace external_service_support {

const char TracingClient::kTracingServiceName[] = "perfetto";

// Initializes perfetto tracing.
//
// If this is called more than once, it will not cause problems because
// because perfetto::Tracing::Initialize() handles this internally.
void InitializePerfettoTracing(base::tracing::PerfettoPlatform* platform) {
  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  args.platform = platform;
  perfetto::Tracing::Initialize(args);
}

PerfettoTracingClient::~PerfettoTracingClient() = default;

PerfettoTracingClient::PerfettoTracingClient()
    : perfetto_platform_(std::make_unique<base::tracing::PerfettoPlatform>(
          base::tracing::PerfettoPlatform::TaskRunnerType::kBuiltin)) {
  InitializePerfettoTracing(perfetto_platform_.get());
}

std::unique_ptr<TracingClient> TracingClient::Create(
    ExternalConnector* connector) {
  return std::make_unique<PerfettoTracingClient>();
}

}  // namespace external_service_support
}  // namespace chromecast
