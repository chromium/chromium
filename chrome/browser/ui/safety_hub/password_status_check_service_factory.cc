// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"

#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/common/chrome_features.h"

// static
PasswordStatusCheckServiceFactory*
PasswordStatusCheckServiceFactory::GetInstance() {
  return base::Singleton<PasswordStatusCheckServiceFactory>::get();
}

// static
PasswordStatusCheckService* PasswordStatusCheckServiceFactory::GetForProfile(
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return nullptr;
  }
  return static_cast<PasswordStatusCheckService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordStatusCheckServiceFactory::PasswordStatusCheckServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasswordStatusCheckService",
          ProfileSelections::Builder()
              // There is no need for create this for OTR profiles as there is
              // no OTR password manager. However, it is reasonable to use the
              // data the service provides in an OTR profile, e.g. for
              // displaying actionable items.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(BulkLeakCheckServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
}

PasswordStatusCheckServiceFactory::~PasswordStatusCheckServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PasswordStatusCheckServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  password_manager::PasswordStoreInterface* store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  // Incognito, guest, or system profiles doesn't have PasswordStore so
  // SafetyHub shouldn't be created as well.
  if (!store) {
    return nullptr;
  }

  return std::make_unique<PasswordStatusCheckService>(
      Profile::FromBrowserContext(context));
}
