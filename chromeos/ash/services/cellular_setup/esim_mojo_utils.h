// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_

#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash::cellular_setup {

// Returns the mojo ProfileInstallResult status corresponding to
// HermesResponseStatus from D-Bus clients.
mojom::ProfileInstallResult InstallResultFromStatus(
    HermesResponseStatus status);

// Returns mojo ProfileState corresponding to state CellularESimProfile object.
mojom::ProfileState ProfileStateToMojo(CellularESimProfile::State state);

// Returns mojo ESimOperationResult corresponding to response status
// from D-Bus clients.
mojom::ESimOperationResult OperationResultFromStatus(
    HermesResponseStatus status);

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_
