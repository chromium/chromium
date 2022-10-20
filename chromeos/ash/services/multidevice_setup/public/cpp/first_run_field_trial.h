// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FIRST_RUN_FIELD_TRIAL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FIRST_RUN_FIELD_TRIAL_H_

namespace base {
class FeatureList;
}  // namespace base

namespace ash {

namespace multidevice_setup {

// Sets up multi-device features for the first run flow (i.e., OOBE/login). This
// ensures that multi-device features can be featured in OOBE but can later be
// disabled by Finch when appropriate.
void CreateFirstRunFieldTrial(base::FeatureList* feature_list);

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FIRST_RUN_FIELD_TRIAL_H_
