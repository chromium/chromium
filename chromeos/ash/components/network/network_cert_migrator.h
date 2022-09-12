// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_MIGRATOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_MIGRATOR_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkStateHandler;

// Migrates network configurations with incorrect or missing slot IDs of client
// certificates.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkCertMigrator
    : public NetworkStateHandlerObserver,
      public NetworkCertLoader::Observer {
 public:
  NetworkCertMigrator(const NetworkCertMigrator&) = delete;
  NetworkCertMigrator& operator=(const NetworkCertMigrator&) = delete;

  ~NetworkCertMigrator() override;

 private:
  friend class NetworkHandler;
  friend class NetworkCertMigratorTest;
  class MigrationTask;

  NetworkCertMigrator();
  void Init(NetworkStateHandler* network_state_handler);

  // NetworkStateHandlerObserver overrides
  void NetworkListChanged() override;

  // NetworkCertLoader::Observer overrides
  void OnCertificatesLoaded() override;

  // Unowned associated NetworkStateHandler* (global or test instance).
  NetworkStateHandler* network_state_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<NetworkCertMigrator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_MIGRATOR_H_
