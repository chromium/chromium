// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_
#define CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

// Migrates network configurations with incorrect or missing slot IDs of client
// certificates.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkCertMigrator
    : public NetworkStateHandlerObserver,
      public NetworkCertLoader::Observer {
 public:
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

  base::WeakPtrFactory<NetworkCertMigrator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkCertMigrator);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_
