// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_SETTINGS_PROVIDER_H_
#define CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_SETTINGS_PROVIDER_H_

#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// A IdentifiabilityStudySettingsProvider that's based on the identifiability
// study feature flags and field trial configuration.
//
// These features and field trial parameters are found in
// privacy_budget_features.h.
//
// In the browser process these settings are used to filter out metrics that
// should be excluded from the study. In the renderer they are used to prevent
// certain surfaces from being sampled at all.
//
// Note: The ONLY parameters that should be exposed to the renderer are those
//   that are shared across a large number of clients since it is possible to
//   indirectly observe the set of surfaces that are sampled. Thus the set of
//   sampled surfaces itself could become an identifier.
//
//   Renderer-exposed controls are meant to act as a granular kill switches in
//   cases where the act of sampling itself has unforeseen adverse side-effects.
//   Filtering done in the browser are privacy controls and cannot address
//   issues arising at the point of sampling.
class PrivacyBudgetSettingsProvider final
    : public blink::IdentifiabilityStudySettingsProvider {
 public:
  explicit PrivacyBudgetSettingsProvider(bool meta_experiment_active);
  PrivacyBudgetSettingsProvider(const PrivacyBudgetSettingsProvider&);
  PrivacyBudgetSettingsProvider(PrivacyBudgetSettingsProvider&&);
  ~PrivacyBudgetSettingsProvider() override;

  bool operator=(const PrivacyBudgetSettingsProvider&) const = delete;
  bool operator=(const PrivacyBudgetSettingsProvider&&) const = delete;

  // blink::IdentifiabilityStudySettingsProvider
  bool IsMetaExperimentActive() const override;
  bool IsActive() const override;
  bool IsAnyTypeOrSurfaceBlocked() const override;
  bool IsSurfaceAllowed(blink::IdentifiableSurface surface) const override;
  bool IsTypeAllowed(blink::IdentifiableSurface::Type type) const override;

 private:
  // Set of identifiable surfaces for which we will NOT collect metrics. This
  // list is server controlled.
  const IdentifiableSurfaceSet blocked_surfaces_;

  // Set of identifiable surface types for which we will NOT collect metrics.
  // This list is server controlled.
  const IdentifiableSurfaceTypeSet blocked_types_;

  // True if identifiability study is enabled.
  const bool enabled_ = false;

  // True if the meta experiment is enabled for this client.
  const bool meta_experiment_active_ = false;
};

#endif  // CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_SETTINGS_PROVIDER_H_
