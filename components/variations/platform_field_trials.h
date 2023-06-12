// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
#define COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_

#include "base/component_export.h"
#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"

namespace variations {

// Infrastructure for setting up platform specific field trials. Chrome and
// WebView make use through their corresponding subclasses.
class COMPONENT_EXPORT(VARIATIONS) PlatformFieldTrials {
 public:
  PlatformFieldTrials() = default;

  PlatformFieldTrials(const PlatformFieldTrials&) = delete;
  PlatformFieldTrials& operator=(const PlatformFieldTrials&) = delete;

  virtual ~PlatformFieldTrials() = default;

  // Called once trials are set up to do platform-specific initialization.
  // Mainly used for setting up persistent histograms.
  virtual void OnVariationsSetupComplete() {}

  // Create field trials that are defined by client code rather than being sent
  // from the variations server. Called after server trials are set up.
  // |has_seed| indicates that the variations service used a seed to create
  // field trials. This can be used to prevent associating a field trial with a
  // feature that you expect to be controlled by the variations seed.
  // |entropy_providers| should be used for randomizing trials. Trials that
  // should be visible to Google web properties should use the low_entropy()
  // provider.
  virtual void SetUpClientSideFieldTrials(
      bool has_seed,
      const variations::EntropyProviders& entropy_providers,
      base::FeatureList* feature_list) {}

  // Registers any synthetic field trials. Will be called later than the above
  // methods, in particular after g_browser_process is available..
  virtual void RegisterSyntheticTrials() {}

  // Registers feature overrides. Called after server trials and client side
  // trials are set up, before initializing the singleton feature list. This
  // can be used to override a feature after a field trial you expect to
  // control. For example, this mechanism can be used to provide
  // different per-platform feature defaults for platforms that are compiled
  // together, like Android WebView and Android Chrome.
  virtual void RegisterFeatureOverrides(base::FeatureList* feature_list) {}
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
