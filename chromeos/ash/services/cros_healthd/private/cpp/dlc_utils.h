// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DLC_UTILS_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DLC_UTILS_H_

namespace ash::cros_healthd::internal {

// Triggers the installation of required DLCs for cros_healthd.
void TriggerDlcInstall();

}  // namespace ash::cros_healthd::internal

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DLC_UTILS_H_
