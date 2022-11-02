// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

namespace named_mojo_ipc_server {

// static
// Dummy implementation that returns nullptr for unsupported platforms, i.e.
// Mac.
// TODO(yuweih): Implement NamedMojoServerEndpointConnector for Mac.
std::unique_ptr<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(Delegate* delegate) {
  return nullptr;
}

}  // namespace named_mojo_ipc_server
