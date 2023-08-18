// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_configuration_provider.h"

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/user_education_features.h"

// Implemented in chrome/browser/ui/views/user_education.
extern void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);

UserEducationConfigurationProvider::UserEducationConfigurationProvider() {
  MaybeRegisterChromeFeaturePromos(registry_);
}

UserEducationConfigurationProvider::~UserEducationConfigurationProvider() =
    default;

bool UserEducationConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    feature_engagement::FeatureConfig& config,
    const feature_engagement::FeatureVector& known_features,
    const feature_engagement::GroupVector& known_groups) const {
  // Never override existing configurations unless 2.0 is enabled.
  if (config.valid &&
      !base::FeatureList::IsEnabled(
          user_education::features::kUserEducationExperienceVersion2)) {
    return false;
  }

  // TODO(dfried): apply configuration from IPH registry.
  return false;
}

const char*
UserEducationConfigurationProvider::GetConfigurationSourceDescription() const {
  return "Browser User Education";
}
