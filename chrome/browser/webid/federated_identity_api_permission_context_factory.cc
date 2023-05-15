// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_api_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_api_permission_context.h"

// static
FederatedIdentityApiPermissionContext*
FederatedIdentityApiPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityApiPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityApiPermissionContextFactory*
FederatedIdentityApiPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FederatedIdentityApiPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentityApiPermissionContextFactory::
    FederatedIdentityApiPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "FederatedIdentityApiPermissionContext",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityApiPermissionContextFactory::
    ~FederatedIdentityApiPermissionContextFactory() = default;

KeyedService*
FederatedIdentityApiPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityApiPermissionContext(profile);
}
