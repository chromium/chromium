// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace multidevice_setup {

// Fake mojom::HostStatusObserver implementation for tests.
class FakeHostStatusObserver : public mojom::HostStatusObserver {
 public:
  FakeHostStatusObserver();
  ~FakeHostStatusObserver() override;

  mojo::PendingRemote<mojom::HostStatusObserver> GenerateRemote();

  const std::vector<
      std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>>&
  host_status_updates() const {
    return host_status_updates_;
  }

 private:
  // mojom::HostStatusObserver:
  void OnHostStatusChanged(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDevice>& host_device) override;

  std::vector<
      std::pair<mojom::HostStatus, base::Optional<multidevice::RemoteDevice>>>
      host_status_updates_;

  mojo::ReceiverSet<mojom::HostStatusObserver> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostStatusObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_
