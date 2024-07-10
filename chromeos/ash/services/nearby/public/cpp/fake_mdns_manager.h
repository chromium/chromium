// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_MDNS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_MDNS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::nearby {

// An implementation of MdnsManager for use in unit tests.
class FakeMdnsManager : public ::sharing::mojom::MdnsManager {
 public:
  FakeMdnsManager();
  ~FakeMdnsManager() override;

  // ::sharing::mojom::MdnsManager
  void StartDiscoverySession(const std::string& service_type,
                             StartDiscoverySessionCallback callback) override;
  void StopDiscoverySession(const std::string& service_type,
                            StopDiscoverySessionCallback callback) override;
  void AddObserver(
      ::mojo::PendingRemote<::sharing::mojom::MdnsObserver> observer) override;

  void NotifyObserversServiceFound(
      ::sharing::mojom::NsdServiceInfoPtr service_info);
  void NotifyObserversServiceLost(
      ::sharing::mojom::NsdServiceInfoPtr service_info);

 private:
  base::flat_set<std::string> discovery_sessions_;
  mojo::RemoteSet<::sharing::mojom::MdnsObserver> observers_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_MDNS_MANAGER_H_
