// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_annotations/user_annotations_service_factory.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace {

bool IsEphemeralProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return true;
  }
#endif

  // Catch additional logic that may not be caught by the existing Ash check.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->IsEphemeral();
}

}  // namespace

// static
user_annotations::UserAnnotationsService*
UserAnnotationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<user_annotations::UserAnnotationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
UserAnnotationsServiceFactory* UserAnnotationsServiceFactory::GetInstance() {
  static base::NoDestructor<UserAnnotationsServiceFactory> instance;
  return instance.get();
}

UserAnnotationsServiceFactory::UserAnnotationsServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserAnnotationsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

UserAnnotationsServiceFactory::~UserAnnotationsServiceFactory() = default;

std::unique_ptr<KeyedService>
UserAnnotationsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!user_annotations::IsUserAnnotationsEnabled()) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);

  // This is not useful in kiosk or ephemeral profile mode, so simply never
  // construct the service for those users.
  if (IsRunningInAppMode() || IsEphemeralProfile(profile)) {
    return nullptr;
  }

  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!ogks) {
    return nullptr;
  }

  return std::make_unique<user_annotations::UserAnnotationsService>(
      ogks, profile->GetPath(), g_browser_process->os_crypt_async(), ogks);
}
