// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"

#include "base/task/sequenced_task_runner.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"

namespace named_mojo_ipc_server {
namespace {
// The amount of time to wait before retrying |TryStart| if it failed.
static constexpr base::TimeDelta kRetryStartDelay = base::Seconds(3);
}  // namespace

NamedMojoServerEndpointConnector::NamedMojoServerEndpointConnector(
    const EndpointOptions& options,
    base::SequenceBound<Delegate> delegate)
    : options_(options), delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

NamedMojoServerEndpointConnector::~NamedMojoServerEndpointConnector() = default;

void NamedMojoServerEndpointConnector::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!TryStart()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NamedMojoServerEndpointConnector::Start,
                       weak_ptr_factory_.GetWeakPtr()),
        kRetryStartDelay);
    return;
  }
}

}  // namespace named_mojo_ipc_server
