// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/utility/cast_content_utility_client.h"

namespace chromecast {
namespace shell {

CastContentUtilityClient::CastContentUtilityClient() = default;

bool CastContentUtilityClient::HandleServiceRequestDeprecated(
    const std::string& service_name,
    mojo::ScopedMessagePipeHandle service_pipe) {
  return HandleServiceRequest(
      service_name, mojo::PendingReceiver<service_manager::mojom::Service>(
                        std::move(service_pipe)));
}

bool CastContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  return false;
}

}  // namespace shell
}  // namespace chromecast
