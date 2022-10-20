// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/first_run_field_trial.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/system/sys_info.h"

namespace ash {

namespace multidevice_setup {

namespace {

// Note: The trial name *must* match the study name in the server-side
// configuration.
const char kInstantTetheringTrialName[] = "Instant Tethering";
const char kInstantTetheringGroupName[] = "InstantTethering_FirstRunTrial";

constexpr const char* const kBoardsToEnableInstantTethering[] = {"eve",
                                                                 "nocturne"};

}  // namespace

void CreateFirstRunFieldTrial(base::FeatureList* feature_list) {
  // If the hardware name of the current device is not one of the board names in
  // |kBoardsToEnableInstantTethering|, nothing needs to be done.
  if (!base::Contains(kBoardsToEnableInstantTethering,
                      base::SysInfo::HardwareModelName())) {
    return;
  }

  // Instant Tethering is controlled by a flag which is off by default. The flag
  // is enabled via a server-side configuration for some devices, but these
  // configurations do not take effect on the first run. For those devices,
  // enable Instant Tethering for this first run; on subsequent runs, the
  // server-side configuration will be used instead.
  scoped_refptr<base::FieldTrial> trial(base::FieldTrialList::CreateFieldTrial(
      kInstantTetheringTrialName, kInstantTetheringGroupName));
  if (trial.get()) {
    feature_list->RegisterFieldTrialOverride(
        features::kInstantTethering.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  }
}

}  // namespace multidevice_setup

}  // namespace ash
