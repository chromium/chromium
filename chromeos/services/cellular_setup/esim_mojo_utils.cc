// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_mojo_utils.h"

#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"

namespace chromeos {
namespace cellular_setup {

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

mojom::ProfileState ProfileStateToMojo(hermes::profile::State state) {
  switch (state) {
    case hermes::profile::State::kActive:
      return mojom::ProfileState::kActive;
    case hermes::profile::State::kInactive:
      return mojom::ProfileState::kInactive;
    case hermes::profile::State::kPending:
      return mojom::ProfileState::kPending;
  }
  NOTREACHED() << "Cannot convert invalid hermes profile state "
               << static_cast<int>(state);
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

}  // namespace cellular_setup
}  // namespace chromeos