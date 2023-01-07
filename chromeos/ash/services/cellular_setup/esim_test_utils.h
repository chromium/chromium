// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_

#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cellular_setup {

// Calls GetProperties on a remote euicc object and waits for
// result. Returns the resulting EuiccProperties structure.
mojom::EuiccPropertiesPtr GetEuiccProperties(
    const mojo::Remote<mojom::Euicc>& euicc);

// Calls GetProperties on a remote esim_profile object and waits
// for result. Returns the resulting EuiccProperties structure.
mojom::ESimProfilePropertiesPtr GetESimProfileProperties(
    const mojo::Remote<mojom::ESimProfile>& esim_profile);

// Calls GetProfileList on a remote euicc object and waits
// for result. Returns the resulting list of ESimProfile
// pending remotes.
std::vector<mojo::PendingRemote<mojom::ESimProfile>> GetProfileList(
    const mojo::Remote<mojom::Euicc>& euicc);

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_
