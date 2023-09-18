// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_configuration_provider.h"

#include <cstring>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/user_education_features.h"

namespace {

std::string FeatureNameToEventName(const base::Feature& feature) {
  constexpr char kIPHPrefix[] = "IPH_";
  std::string name = feature.name;
  if (base::StartsWith(name, kIPHPrefix)) {
    name = name.substr(strlen(kIPHPrefix));
  }
  return name;
}

bool ShouldOverwriteValidConfigurations() {
  return base::FeatureList::IsEnabled(
      user_education::features::kUserEducationExperienceVersion2);
}

}  // namespace

// Implemented in chrome/browser/ui/views/user_education.
extern void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);

UserEducationConfigurationProvider::UserEducationConfigurationProvider()
    : overwrite_valid_configurations_(ShouldOverwriteValidConfigurations()) {
  MaybeRegisterChromeFeaturePromos(registry_);
}

UserEducationConfigurationProvider::UserEducationConfigurationProvider(
    user_education::FeaturePromoRegistry registry_for_testing)
    : registry_(std::move(registry_for_testing)),
      overwrite_valid_configurations_(ShouldOverwriteValidConfigurations()) {}

UserEducationConfigurationProvider::~UserEducationConfigurationProvider() =
    default;

bool UserEducationConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    feature_engagement::FeatureConfig& config,
    const feature_engagement::FeatureVector& known_features,
    const feature_engagement::GroupVector& known_groups) const {

  // Never override existing configurations unless 2.0 is enabled.
  if (config.valid && !overwrite_valid_configurations_) {
    return false;
  }

  if (!registry_.IsFeatureRegistered(feature)) {
    return false;
  }

  const auto* const promo_spec = registry_.GetParamsForFeature(feature);
  const bool is_unlimited =
      promo_spec->promo_subtype() ==
          user_education::FeaturePromoSpecification::PromoSubtype::kPerApp ||
      promo_spec->promo_subtype() ==
          user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice;

  switch (promo_spec->promo_type()) {
    case user_education::FeaturePromoSpecification::PromoType::kToast:
      // Toasts can always show and do not impact other IPH.
      config.session_rate.type = feature_engagement::ANY;
      config.session_rate.value = 0;
      config.session_rate_impact.type =
          feature_engagement::SessionRateImpact::Type::NONE;
      config.session_rate_impact.affected_features.reset();
      break;

    case user_education::FeaturePromoSpecification::PromoType::kSnooze:
    case user_education::FeaturePromoSpecification::PromoType::kCustomAction:
    case user_education::FeaturePromoSpecification::PromoType::kTutorial:
      if (is_unlimited) {
        config.session_rate.type = feature_engagement::ANY;
        config.session_rate_impact.type =
            feature_engagement::SessionRateImpact::Type::ALL;
        config.session_rate_impact.affected_features.reset();
      } else {
        // Heavyweight IPH can only show once per session.
        config.session_rate.type = feature_engagement::EQUAL;
        config.session_rate.value = 0;
        config.session_rate_impact.type =
            feature_engagement::SessionRateImpact::Type::ALL;
        config.session_rate_impact.affected_features.reset();
      }
      break;

    case user_education::FeaturePromoSpecification::PromoType::kLegacy:
    case user_education::FeaturePromoSpecification::PromoType::kUnspecified:
      // No configuration is provided for legacy IPH.
      CHECK(!overwrite_valid_configurations_)
          << "Legacy promos not allowed in User Education Experience V2.";
      return false;
  }

  // All IPH block all other IPH.
  config.blocked_by.type = feature_engagement::BlockedBy::Type::ALL;
  config.blocked_by.affected_features.reset();
  config.blocking.type = feature_engagement::Blocking::Type::ALL;

  // Set up a default trigger if one is not present.
  if (config.trigger.name.empty()) {
    config.trigger.name = GetDefaultTriggerName(feature);
  }
  if (is_unlimited) {
    config.trigger.comparator.type = feature_engagement::ANY;
    config.trigger.comparator.value = 0;
  } else {
    config.trigger.comparator.type = feature_engagement::LESS_THAN;
    config.trigger.comparator.value = 3;
  }
  config.trigger.storage = feature_engagement::kMaxStoragePeriod;
  config.trigger.window = feature_engagement::kMaxStoragePeriod;

  // Set up a default "used" event if one is not present.
  if (config.used.name.empty()) {
    config.used.name = GetDefaultUsedName(feature);
  }
  config.used.comparator.type = feature_engagement::EQUAL;
  config.used.comparator.value = 0;
  config.used.storage = feature_engagement::kMaxStoragePeriod;
  config.used.window = feature_engagement::kMaxStoragePeriod;

  // Unless there are additional constraints, the IPH should be available
  // immediately; otherwise wait 7 days.
  if (config.event_configs.empty()) {
    config.availability.type = feature_engagement::ANY;
    config.availability.value = 0;
  } else {
    config.availability.type = feature_engagement::GREATER_THAN_OR_EQUAL;
    config.availability.value = 7;
  }

  config.valid = true;

  return true;
}

const char*
UserEducationConfigurationProvider::GetConfigurationSourceDescription() const {
  return "Browser User Education";
}

// static
std::string UserEducationConfigurationProvider::GetDefaultTriggerName(
    const base::Feature& feature) {
  return FeatureNameToEventName(feature) + "_trigger";
}

std::string UserEducationConfigurationProvider::GetDefaultUsedName(
    const base::Feature& feature) {
  return FeatureNameToEventName(feature) + "_used";
}
