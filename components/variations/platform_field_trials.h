// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
#define COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_

#include "base/metrics/field_trial.h"

namespace variations {

// Infrastructure for setting up platform specific field trials. Chrome and
// WebView make use through their corresponding subclasses.
class PlatformFieldTrials {
 public:
  PlatformFieldTrials() = default;
  virtual ~PlatformFieldTrials() = default;

  // Set up field trials for a specific platform.
  virtual void SetupFieldTrials() = 0;

  // Create field trials that will control feature list features. This should be
  // called during the same timing window as
  // FeatureList::AssociateReportingFieldTrial. |has_seed| indicates that the
  // variations service used a seed to create field trials. This can be used to
  // prevent associating a field trial with a feature that you expect to be
  // controlled by the variations seed.
  virtual void SetupFeatureControllingFieldTrials(
      bool has_seed,
      base::FeatureList* feature_list) = 0;

  // Register any synthetic field trials. Will be called later than the above
  // methods, in particular after g_browser_process is available..
  virtual void RegisterSyntheticTrials() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformFieldTrials);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
