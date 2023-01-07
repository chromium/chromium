// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/external_mojo/external_service_support/tracing_client_dummy.h"

namespace chromecast {
namespace external_service_support {

// static
const char TracingClient::kTracingServiceName[] = "unknown";

// static
std::unique_ptr<TracingClient> TracingClient::Create(
    ExternalConnector* connector) {
  return std::make_unique<TracingClientDummy>();
}

}  // namespace external_service_support
}  // namespace chromecast
