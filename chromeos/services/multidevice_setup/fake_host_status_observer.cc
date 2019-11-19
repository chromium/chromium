// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_host_status_observer.h"

namespace chromeos {

namespace multidevice_setup {

FakeHostStatusObserver::FakeHostStatusObserver() = default;

FakeHostStatusObserver::~FakeHostStatusObserver() = default;

mojo::PendingRemote<mojom::HostStatusObserver>
FakeHostStatusObserver::GenerateRemote() {
  mojo::PendingRemote<mojom::HostStatusObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeHostStatusObserver::OnHostStatusChanged(
    mojom::HostStatus host_status,
    const base::Optional<multidevice::RemoteDevice>& host_device) {
  host_status_updates_.emplace_back(host_status, host_device);
}

}  // namespace multidevice_setup

}  // namespace chromeos
