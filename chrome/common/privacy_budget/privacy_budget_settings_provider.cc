// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

PrivacyBudgetSettingsProvider::PrivacyBudgetSettingsProvider()
    : blocked_surfaces_(
          DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceSet>(
              features::kIdentifiabilityStudyBlockedMetrics.Get())),
      blocked_types_(
          DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeSet>(
              features::kIdentifiabilityStudyBlockedTypes.Get())),
      per_surface_sample_rates_(
          DecodeIdentifiabilityFieldTrialParam<SurfaceSampleRateMap>(
              features::kIdentifiabilityStudyPerSurfaceSampleRates.Get())),
      per_type_sample_rates_(
          DecodeIdentifiabilityFieldTrialParam<TypeSampleRateMap>(
              features::kIdentifiabilityStudyPerTypeSampleRates.Get())),

      // In practice there's really no point in enabling the feature with a max
      // active surface count of 0.
      enabled_(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy) &&
               features::kIdentifiabilityStudySurfaceSelectionRate.Get() > 0) {}

PrivacyBudgetSettingsProvider::PrivacyBudgetSettingsProvider(
    const PrivacyBudgetSettingsProvider&) = default;
PrivacyBudgetSettingsProvider::PrivacyBudgetSettingsProvider(
    PrivacyBudgetSettingsProvider&&) = default;
PrivacyBudgetSettingsProvider::~PrivacyBudgetSettingsProvider() = default;

bool PrivacyBudgetSettingsProvider::IsActive() const {
  return enabled_;
}

bool PrivacyBudgetSettingsProvider::IsAnyTypeOrSurfaceBlocked() const {
  return !blocked_surfaces_.empty() || !blocked_types_.empty() ||
         base::ranges::any_of(per_surface_sample_rates_,
                              [](const auto& p) { return p.second == 0; }) ||
         base::ranges::any_of(per_type_sample_rates_,
                              [](const auto& p) { return p.second == 0; });
}

bool PrivacyBudgetSettingsProvider::IsSurfaceAllowed(
    blink::IdentifiableSurface surface) const {
  return !base::Contains(blocked_surfaces_, surface) &&
         IsTypeAllowed(surface.GetType()) && SampleRateImpl(surface) > 0;
}

bool PrivacyBudgetSettingsProvider::IsTypeAllowed(
    blink::IdentifiableSurface::Type type) const {
  return !base::Contains(blocked_types_, type) && SampleRateImpl(type) > 0;
}

int PrivacyBudgetSettingsProvider::SampleRate(
    blink::IdentifiableSurface surface) const {
  if (base::Contains(blocked_surfaces_, surface) ||
      base::Contains(blocked_types_, surface.GetType())) {
    return 0;
  }
  return SampleRateImpl(surface);
}

int PrivacyBudgetSettingsProvider::SampleRate(
    blink::IdentifiableSurface::Type type) const {
  if (base::Contains(blocked_types_, type))
    return 0;
  return SampleRateImpl(type);
}

int PrivacyBudgetSettingsProvider::SampleRateImpl(
    blink::IdentifiableSurface surface) const {
  auto it = per_surface_sample_rates_.find(surface);
  if (it != per_surface_sample_rates_.end())
    return it->second;

  return SampleRateImpl(surface.GetType());
}

int PrivacyBudgetSettingsProvider::SampleRateImpl(
    blink::IdentifiableSurface::Type type) const {
  auto it = per_type_sample_rates_.find(type);
  if (it != per_type_sample_rates_.end())
    return it->second;

  return 1;
}
