// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/first_run_field_trial.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/variations/service/variations_field_trial_creator.h"

namespace chromeos {

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
  if (!base::Contains(
          kBoardsToEnableInstantTethering,
          variations::VariationsFieldTrialCreator::GetShortHardwareClass())) {
    return;
  }

  // Instant Tethering is controlled by a flag which is off by default. The flag
  // is enabled via a server-side configuration for some devices, but these
  // configurations do not take effect on the first run. For those devices,
  // enable Instant Tethering for this first run; on subsequent runs, the
  // server-side configuration will be used instead.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kInstantTetheringTrialName, 100 /* total_probability */,
          kInstantTetheringGroupName, base::FieldTrial::ONE_TIME_RANDOMIZED,
          nullptr /* default_group_number */));
  feature_list->RegisterFieldTrialOverride(
      features::kInstantTethering.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
}

}  // namespace multidevice_setup

}  // namespace chromeos
