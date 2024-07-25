// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace cfm {

FakeServiceConnectionImpl::FakeServiceConnectionImpl() = default;
FakeServiceConnectionImpl::~FakeServiceConnectionImpl() = default;

// Bind to the CfM Service Context Daemon
void FakeServiceConnectionImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver) {

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
            base::BindOnce(&FakeServiceConnectionImpl::CfMContextServiceStarted,
                     base::Unretained(this), std::move(pending_receiver),
                     true));
}

void FakeServiceConnectionImpl::CfMContextServiceStarted(
    mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
    bool is_available) {

  if (callback_.is_null()) {
    pending_receiver.reset();
    return;
  }

  if (!is_available) {
    std::move(callback_).Run(std::move(pending_receiver), false);
    return;
  }

  // Fake that the mojo connection has been successfully established.
  auto bootstrap_mojo_connection_callback =
      base::BindOnce(std::move(callback_), std::move(pending_receiver));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(bootstrap_mojo_connection_callback), true));
}

void FakeServiceConnectionImpl::SetCallback(FakeBootstrapCallback callback) {
  callback_ = std::move(callback);
}

}  // namespace cfm
}  // namespace chromeos
