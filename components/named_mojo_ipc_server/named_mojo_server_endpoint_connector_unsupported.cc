// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace named_mojo_ipc_server {

// static
// Dummy implementation that returns nullptr for unsupported platforms, i.e.
// Mac.
// TODO(yuweih): Implement NamedMojoServerEndpointConnector for Mac.
base::SequenceBound<NamedMojoServerEndpointConnector>
NamedMojoServerEndpointConnector::Create(
    base::SequenceBound<Delegate>,
    scoped_refptr<base::SequencedTaskRunner>) {
  return base::SequenceBound<NamedMojoServerEndpointConnector>();
}

}  // namespace named_mojo_ipc_server
