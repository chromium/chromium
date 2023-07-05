// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PREF_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PREF_UTILS_H_

class PrefService;

namespace private_computing {
class GetStatusResponse;
class SaveStatusRequest;
}  // namespace private_computing

namespace ash::report::utils {

// Handler called after restoring the fresnel pref values from preserved file.
void RestoreLocalStateWithPreservedFile(
    PrefService* local_state,
    private_computing::GetStatusResponse response);

// Use the local state values to create the proto that we save across powerwash.
private_computing::SaveStatusRequest CreatePreservedFileContents(
    PrefService* local_state);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_PREF_UTILS_H_
