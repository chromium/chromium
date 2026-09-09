// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"
#include "components/feature_engagement/public/feature_constants.h"

// static
SearchPromotionManager* SearchPromotionManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchPromotionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchPromotionManagerFactory* SearchPromotionManagerFactory::GetInstance() {
  static base::NoDestructor<SearchPromotionManagerFactory> instance;
  return instance.get();
}

SearchPromotionManagerFactory::SearchPromotionManagerFactory()
    : ProfileKeyedServiceFactory(
          "SearchPromotionManager",
          ProfileSelections::Builder()
              // Search promotions are enabled for regular profiles (disabled in
              // incognito and guest profiles).
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
}

SearchPromotionManagerFactory::~SearchPromotionManagerFactory() = default;

std::unique_ptr<KeyedService>
SearchPromotionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHSearchPromotionFeature)) {
    return nullptr;
  }

  return std::make_unique<SearchPromotionManager>(
      *Profile::FromBrowserContext(context),
      /*create_task_runner_callback=*/base::BindRepeating([]() {
        return std::make_unique<platform_experience::DelegatedTaskRunner>();
      }));
}

// We initialize eagerly to trigger the asynchronous segmentation query on
// startup, ensuring the result is cached by the time the user navigates.
bool SearchPromotionManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(
      feature_engagement::kIPHSearchPromotionFeature);
}
