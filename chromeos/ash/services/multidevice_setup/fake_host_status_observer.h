// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace multidevice_setup {

// Fake mojom::HostStatusObserver implementation for tests.
class FakeHostStatusObserver : public mojom::HostStatusObserver {
 public:
  FakeHostStatusObserver();

  FakeHostStatusObserver(const FakeHostStatusObserver&) = delete;
  FakeHostStatusObserver& operator=(const FakeHostStatusObserver&) = delete;

  ~FakeHostStatusObserver() override;

  mojo::PendingRemote<mojom::HostStatusObserver> GenerateRemote();

  const std::vector<
      std::pair<mojom::HostStatus, absl::optional<multidevice::RemoteDevice>>>&
  host_status_updates() const {
    return host_status_updates_;
  }

 private:
  // mojom::HostStatusObserver:
  void OnHostStatusChanged(
      mojom::HostStatus host_status,
      const absl::optional<multidevice::RemoteDevice>& host_device) override;

  std::vector<
      std::pair<mojom::HostStatus, absl::optional<multidevice::RemoteDevice>>>
      host_status_updates_;

  mojo::ReceiverSet<mojom::HostStatusObserver> receivers_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_OBSERVER_H_
