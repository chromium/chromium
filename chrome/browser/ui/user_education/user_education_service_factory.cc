// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/user_education_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

UserEducationServiceFactory* UserEducationServiceFactory::GetInstance() {
  return base::Singleton<UserEducationServiceFactory>::get();
}

// static
UserEducationService* UserEducationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserEducationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEducationServiceFactory::UserEducationServiceFactory()
    : ProfileKeyedServiceFactory("UserEducationService") {}

UserEducationServiceFactory::~UserEducationServiceFactory() = default;

KeyedService* UserEducationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UserEducationService();
}
