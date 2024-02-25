// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_

#include "base/component_export.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::mojo_service_manager {

// Connects to the mojo service manager. Returns false if cannot connect.
// This will will block until connects to the socket of service manager.
// Note that the service manager also acts as the mojo broker process. Will
// raise a |CHECK(false)| if the service manager disconnected unexpectedly,
// because the mojo cannot work without a broker.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
bool BootstrapServiceManagerConnection();

// Returns whether the mojo remote is bound to the mojo service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
bool IsServiceManagerBound();

// Resets the connection to the mojo service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
void ResetServiceManagerConnection();

// Returns the interface to access the service manager.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
chromeos::mojo_service_manager::mojom::ServiceManagerProxy*
GetServiceManagerProxy();

// Sets the mojo remote for testing.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
void SetServiceManagerRemoteForTesting(
    mojo::PendingRemote<chromeos::mojo_service_manager::mojom::ServiceManager>
        remote);

// Return a pending receiver which is bound to the service manager proxy in an
// utility process. The pending receiver should be passed to ash via
// |UtilityProcessHost::BindHostReceiver| and be bound to an
// UtilityProcessBridge.
COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER)
mojo::PendingReceiver<chromeos::mojo_service_manager::mojom::ServiceManager>
BootstrapServiceManagerInUtilityProcess();

}  // namespace ash::mojo_service_manager

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_SERVICE_MANAGER_CONNECTION_H_
