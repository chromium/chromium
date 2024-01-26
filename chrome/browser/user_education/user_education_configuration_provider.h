// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_CONFIGURATION_PROVIDER_H_
#define CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_CONFIGURATION_PROVIDER_H_

#include <optional>

#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/user_education/common/feature_promo_registry.h"

// Provides feature engagement configuration based on IPH registered in the
// browser.
class UserEducationConfigurationProvider
    : public feature_engagement::ConfigurationProvider {
 public:
  UserEducationConfigurationProvider();
  explicit UserEducationConfigurationProvider(
      user_education::FeaturePromoRegistry registry_for_testing);
  ~UserEducationConfigurationProvider() override;

  // feature_engagement::ConfigurationProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      feature_engagement::FeatureConfig& config,
      const feature_engagement::FeatureVector& known_features,
      const feature_engagement::GroupVector& known_groups) const override;
  const char* GetConfigurationSourceDescription() const override;

  // For automatic configuration of triggers, derive default "trigger" and
  // "used" names. The default "used" name, in turn, will be used to ensure that
  // teams using IPH will be able to call a method that says "this feature was
  // used" while specifying only the associated `base::Feature`.
  static std::string GetDefaultTriggerName(const base::Feature& feature);
  static std::string GetDefaultUsedName(const base::Feature& feature);

 private:
  user_education::FeaturePromoRegistry registry_;
  const bool use_v2_behavior_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_CONFIGURATION_PROVIDER_H_
