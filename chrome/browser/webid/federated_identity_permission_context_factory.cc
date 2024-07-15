// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"

// static
FederatedIdentityPermissionContext*
FederatedIdentityPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityPermissionContext*
FederatedIdentityPermissionContextFactory::GetForProfileIfExists(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
FederatedIdentityPermissionContextFactory*
FederatedIdentityPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FederatedIdentityPermissionContextFactory> instance;
  return instance.get();
}

FederatedIdentityPermissionContextFactory::
    FederatedIdentityPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "FederatedIdentityPermissionContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

FederatedIdentityPermissionContextFactory::
    ~FederatedIdentityPermissionContextFactory() = default;

std::unique_ptr<KeyedService> FederatedIdentityPermissionContextFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* profile) const {
  return std::make_unique<FederatedIdentityPermissionContext>(profile);
}

bool FederatedIdentityPermissionContextFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // So that we can observe the identity manager.
  return true;
}
