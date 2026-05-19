// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

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
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Search promotions are enabled for regular profiles.
              // They are disabled in incognito and guest profiles.
              //
              // Note: Even though the factory is created for all platforms, the
              // underlying manager is only functional on Windows.
              .Build()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
}

SearchPromotionManagerFactory::~SearchPromotionManagerFactory() = default;

std::unique_ptr<KeyedService>
SearchPromotionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SearchPromotionManager>(
      *Profile::FromBrowserContext(context));
}
