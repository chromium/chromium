// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PSM_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PSM_UTILS_H_

#include <optional>
#include <string>

namespace private_membership::rlwe {
class RlwePlaintextId;
}  // namespace private_membership::rlwe

namespace ash::report::utils {

// Generate the PSM id for a use case and window id, with the high entropy seed.
std::optional<private_membership::rlwe::RlwePlaintextId> GeneratePsmIdentifier(
    const std::string& high_entropy_seed,
    const std::string& psm_use_case_str,
    const std::string& window_id);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PSM_UTILS_H_
