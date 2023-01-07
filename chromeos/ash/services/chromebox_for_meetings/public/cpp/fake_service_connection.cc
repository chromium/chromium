// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace ash {
namespace cfm {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
namespace mojom = ::chromeos::cfm::mojom;

FakeServiceConnectionImpl::FakeServiceConnectionImpl() = default;
FakeServiceConnectionImpl::~FakeServiceConnectionImpl() = default;

// Bind to the CfM Service Context Daemon
void FakeServiceConnectionImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver) {
  CfmHotlineClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&FakeServiceConnectionImpl::CfMContextServiceStarted,
                     base::Unretained(this), std::move(pending_receiver)));
}

void FakeServiceConnectionImpl::CfMContextServiceStarted(
    mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
    bool is_available) {
  if (!is_available || callback_.is_null()) {
    pending_receiver.reset();
    if (!callback_.is_null())
      std::move(callback_).Run(std::move(pending_receiver), false);
    return;
  }

  // The easiest source of fds is opening /dev/null.
  base::File file = base::File(base::FilePath("/dev/null"),
                               base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  DCHECK(file.IsValid());

  CfmHotlineClient::Get()->BootstrapMojoConnection(
      base::ScopedFD(file.TakePlatformFile()),
      base::BindOnce(std::move(callback_), std::move(pending_receiver)));
}

void FakeServiceConnectionImpl::SetCallback(FakeBootstrapCallback callback) {
  callback_ = std::move(callback);
}

}  // namespace cfm
}  // namespace ash
