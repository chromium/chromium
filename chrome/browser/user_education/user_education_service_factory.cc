// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_session_manager_impl.h"
#include "content/public/browser/browser_context.h"

UserEducationServiceFactory* UserEducationServiceFactory::GetInstance() {
  static base::NoDestructor<UserEducationServiceFactory> instance;
  return instance.get();
}

// static
UserEducationService* UserEducationServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<UserEducationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEducationServiceFactory::UserEducationServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserEducationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

UserEducationServiceFactory::~UserEducationServiceFactory() = default;

std::unique_ptr<KeyedService>
UserEducationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto result = std::make_unique<UserEducationService>(
      std::make_unique<BrowserFeaturePromoStorageService>(
          Profile::FromBrowserContext(context)));
  result->feature_promo_session_manager().Init(
      &result->feature_promo_storage_service(),
      disable_idle_polling_
          ? std::make_unique<
                user_education::FeaturePromoSessionManager::IdleObserver>()
          : std::make_unique<user_education::PollingIdleObserver>(),
      std::make_unique<
          user_education::FeaturePromoSessionManager::IdlePolicy>());
  return result;
}
