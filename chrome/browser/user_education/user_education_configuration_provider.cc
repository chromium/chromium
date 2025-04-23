// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "chrome/browser/user_education/user_education_configuration_provider.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_features.h"

namespace {

std::string FeatureNameToEventName(const base::Feature& feature) {
  constexpr char kIPHPrefix[] = "IPH_";
  std::string_view name = feature.name;
  auto remainder = base::RemovePrefix(name, kIPHPrefix);
  return std::string(remainder.value_or(name));
}

// Returns whether a comparator is bounded from above.
bool IsAdditionalConfigBounded(const feature_engagement::EventConfig& config) {
  const auto& comparator = config.comparator;
  return (comparator.value > 0 &&
          comparator.type == feature_engagement::LESS_THAN) ||
         comparator.type == feature_engagement::LESS_THAN_OR_EQUAL ||
         (comparator.value == 0 &&
          comparator.type == feature_engagement::EQUAL);
}

}  // namespace

// Implemented in chrome/browser/ui/views/user_education.
extern void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);

UserEducationConfigurationProvider::UserEducationConfigurationProvider() {
  MaybeRegisterChromeFeaturePromos(registry_);
}

UserEducationConfigurationProvider::UserEducationConfigurationProvider(
    user_education::FeaturePromoRegistry registry_for_testing)
    : registry_(std::move(registry_for_testing)) {}

UserEducationConfigurationProvider::~UserEducationConfigurationProvider() =
    default;

bool UserEducationConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    feature_engagement::FeatureConfig& config,
    const feature_engagement::FeatureVector& known_features,
    const feature_engagement::GroupVector& known_groups) const {

  // Features not controlled by FeaturePromoController are ignored.
  if (!registry_.IsFeatureRegistered(feature)) {
    return false;
  }

  const auto* const promo_spec = registry_.GetParamsForFeature(feature);

  // These are baseline session rate values.
  config.session_rate.type = feature_engagement::ANY;
  config.session_rate.value = 0;
  config.session_rate_impact.type =
      feature_engagement::SessionRateImpact::Type::NONE;
  config.session_rate_impact.affected_features.reset();

  switch (promo_spec->promo_type()) {
    case user_education::FeaturePromoSpecification::PromoType::kSnooze:
    case user_education::FeaturePromoSpecification::PromoType::kCustomAction:
    case user_education::FeaturePromoSpecification::PromoType::kTutorial:
    case user_education::FeaturePromoSpecification::PromoType::kCustomUi:
      // Heavyweight promos prevent future low-priority heavyweight promos.
      config.session_rate_impact.type =
          feature_engagement::SessionRateImpact::Type::ALL;
      break;

    case user_education::FeaturePromoSpecification::PromoType::kToast:
    case user_education::FeaturePromoSpecification::PromoType::kLegacy:
    case user_education::FeaturePromoSpecification::PromoType::kRotating:
      // Toasts and rotating promos can always show and do not impact other IPH.
      break;

    case user_education::FeaturePromoSpecification::PromoType::kUnspecified:
      // Should never get here.
      NOTREACHED();
  }

  // All IPH block all other IPH.
  config.blocked_by.type = feature_engagement::BlockedBy::Type::ALL;
  config.blocked_by.affected_features.reset();
  config.blocking.type = feature_engagement::Blocking::Type::ALL;

  // Set up a default trigger if one is not present.
  if (config.trigger.name.empty()) {
    config.trigger.name = GetDefaultTriggerName(feature);
  }
  config.trigger.comparator.type = feature_engagement::ANY;
  config.trigger.comparator.value = 0;
  config.trigger.storage = feature_engagement::kMaxStoragePeriod;
  config.trigger.window = feature_engagement::kMaxStoragePeriod;

  auto& additional = promo_spec->additional_conditions();

  // Set up a default "used" event if one is not present.
  if (config.used.name.empty()) {
    config.used.name = GetDefaultUsedName(feature);
  }

  // The allowed number of uses is only overridden if it's not set.
  if (config.used.comparator.value == 0 &&
      additional.used_limit().has_value()) {
    config.used.comparator.type = feature_engagement::LESS_THAN_OR_EQUAL;
    config.used.comparator.value = additional.used_limit().value();
  } else if (config.used.comparator.type == feature_engagement::ANY) {
    // However, since the default comparator is (ANY, 0) which is invalid for a
    // "used" event, change it to a reasonable default.
    config.used.comparator.type = feature_engagement::EQUAL;
    config.used.comparator.value = 0;
  }

  // The default window and storage are zero, which are not valid. If these have
  // not been set, set them to reasonable values.
  if (config.used.window == 0) {
    config.used.window = feature_engagement::kMaxStoragePeriod;
  }
  if (config.used.storage < config.used.window) {
    config.used.storage = config.used.window;
  }

  // In V2, since trigger config is overwritten, also remove additional
  // references in the existing event configs.
  std::erase_if(config.event_configs, [&trigger_name = config.trigger.name](
                                          const auto& event_config) {
    return event_config.name == trigger_name;
  });

  // Set up additional constraints, if specified and not overridden in the
  // existing config.
  std::vector<feature_engagement::EventConfig> new_event_configs;
  for (const auto& condition : additional.additional_conditions()) {
    const std::string name = condition.event_name;

    // While the "used" condition can participate in additional constraints, the
    // trigger should not.
    CHECK_NE(config.trigger.name, name);

    // Determine if there's already a config for this event.
    if (std::any_of(config.event_configs.begin(), config.event_configs.end(),
                    [&name](const auto& event_config) {
                      return event_config.name == name;
                    })) {
      continue;
    }

    // Add the additional event configuration.
    feature_engagement::ComparatorType comparator;
    switch (condition.constraint) {
      using Constraint = user_education::FeaturePromoSpecification::
          AdditionalConditions::Constraint;
      case Constraint::kAtLeast:
        comparator = feature_engagement::GREATER_THAN_OR_EQUAL;
        break;
      case Constraint::kAtMost:
        comparator = feature_engagement::LESS_THAN_OR_EQUAL;
        break;
      case Constraint::kExactly:
        comparator = feature_engagement::EQUAL;
        break;
    }
    const size_t window =
        condition.in_days.value_or(feature_engagement::kMaxStoragePeriod);
    new_event_configs.emplace_back(
        name, feature_engagement::Comparator{comparator, condition.count},
        window, window);
  }
  std::copy(new_event_configs.begin(), new_event_configs.end(),
            std::inserter(config.event_configs, config.event_configs.begin()));

  // Set up some reasonable availability values.
  if (config.availability != feature_engagement::Comparator()) {
    // There is already configuration specified for availability other than the
    // default. Make sure the availability is a lower bound.
    if (config.availability.type != feature_engagement::GREATER_THAN) {
      config.availability.type = feature_engagement::GREATER_THAN_OR_EQUAL;
    }
  } else if (additional.initial_delay_days().has_value()) {
    // Explicit availability was set in User Ed config, so use that.
    config.availability.type = feature_engagement::GREATER_THAN_OR_EQUAL;
    config.availability.value = additional.initial_delay_days().value();
  } else if (std::any_of(config.event_configs.begin(),
                         config.event_configs.end(),
                         &IsAdditionalConfigBounded)) {
    // If any of the additional event configs have put a maximum cap on a
    // particular event, then default to a minimum availability period to give
    // enough time to determine if the events in question happened.
    config.availability.type = feature_engagement::GREATER_THAN_OR_EQUAL;
    config.availability.value = 7;
  } else {
    // This should already be the default - the promo will be available as soon
    // as the feature is enabled.
    config.availability.type = feature_engagement::ANY;
    config.availability.value = 0;
  }

  // By this point the configuration should be valid.
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
