// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace named_mojo_ipc_server {

// static
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    scoped_refptr<base::SequencedTaskRunner> io_sequence,
    const mojo::NamedPlatformChannel::ServerName& server_name,
    base::SequenceBound<Delegate> delegate) {
  return base::SequenceBound<NamedMojoServerEndpointConnector>();
}

}  // namespace named_mojo_ipc_server
