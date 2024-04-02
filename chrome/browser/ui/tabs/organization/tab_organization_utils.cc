// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"

TabOrganizationUtils::TabOrganizationUtils() = default;

TabOrganizationUtils::~TabOrganizationUtils() = default;

// static
TabOrganizationUtils* TabOrganizationUtils::GetInstance() {
  static base::NoDestructor<TabOrganizationUtils> instance;
  return instance.get();
}

bool TabOrganizationUtils::IsEnabled(Profile* profile) {
  bool feature_is_enabled = features::IsTabOrganization();
  if (ignore_opt_guide_for_testing_) {
    return feature_is_enabled;
  }
  const OptimizationGuideKeyedService* const opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return opt_guide_keyed_service != nullptr &&
         opt_guide_keyed_service->ShouldFeatureBeCurrentlyEnabledForUser(
             optimization_guide::UserVisibleFeatureKey::kTabOrganization) &&
         feature_is_enabled;
}
