// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_mojo_utils.h"

#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"

namespace ash::cellular_setup {

mojom::ProfileInstallResult InstallResultFromStatus(
    HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      return mojom::ProfileInstallResult::kSuccess;
    case HermesResponseStatus::kErrorNeedConfirmationCode:
      return mojom::ProfileInstallResult::kErrorNeedsConfirmationCode;
    case HermesResponseStatus::kErrorInvalidActivationCode:
      return mojom::ProfileInstallResult::kErrorInvalidActivationCode;
    default:
      // Treat all other status codes as installation failure.
      return mojom::ProfileInstallResult::kFailure;
  }
}

mojom::ProfileState ProfileStateToMojo(CellularESimProfile::State state) {
  switch (state) {
    case CellularESimProfile::State::kActive:
      return mojom::ProfileState::kActive;
    case CellularESimProfile::State::kInactive:
      return mojom::ProfileState::kInactive;
    case CellularESimProfile::State::kPending:
      return mojom::ProfileState::kPending;
    case CellularESimProfile::State::kInstalling:
      return mojom::ProfileState::kInstalling;
  }
  NOTREACHED_IN_MIGRATION()
      << "Cannot convert invalid profile state " << static_cast<int>(state);
  return mojom::ProfileState::kPending;
}

mojom::ESimOperationResult OperationResultFromStatus(
    HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      return mojom::ESimOperationResult::kSuccess;
    default:
      // Treat all other status codes as operation failure.
      return mojom::ESimOperationResult::kFailure;
  }
}

}  // namespace ash::cellular_setup
