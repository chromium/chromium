// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_hats_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/performance_controls/performance_controls_hats_service.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/browser_context.h"

PerformanceControlsHatsServiceFactory::PerformanceControlsHatsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PerformanceControlsHatsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HatsServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PerformanceControlsHatsServiceFactory*
PerformanceControlsHatsServiceFactory::GetInstance() {
  static base::NoDestructor<PerformanceControlsHatsServiceFactory> instance;
  return instance.get();
}

PerformanceControlsHatsService*
PerformanceControlsHatsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PerformanceControlsHatsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
PerformanceControlsHatsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord() ||
      (!base::FeatureList::IsEnabled(
           performance_manager::features::
               kPerformanceControlsPerformanceSurvey) &&
       !base::FeatureList::IsEnabled(
           performance_manager::features::
               kPerformanceControlsBatteryPerformanceSurvey) &&
       !base::FeatureList::IsEnabled(
           performance_manager::features::
               kPerformanceControlsMemorySaverOptOutSurvey) &&
       !base::FeatureList::IsEnabled(
           performance_manager::features::
               kPerformanceControlsBatterySaverOptOutSurvey))) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  // If there is no HaTS service, or the HaTS service reports the user is not
  // eligible to be surveyed by HaTS, do not create the service. This state is
  // unlikely to change over the life of the profile (e.g. before closing
  // Chrome) and simply not creating the service avoids unnecessary work
  // tracking user interactions.
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile,
                                        /*create_if_necessary=*/true);
  if (!hats_service ||
      !hats_service->CanShowAnySurvey(/*user_prompted=*/false)) {
    return nullptr;
  }

  return std::make_unique<PerformanceControlsHatsService>(profile);
}
