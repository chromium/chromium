// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

// static
FederatedIdentityAutoReauthnPermissionContext*
FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityAutoReauthnPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityAutoReauthnPermissionContextFactory*
FederatedIdentityAutoReauthnPermissionContextFactory::GetInstance() {
  static base::NoDestructor<
      FederatedIdentityAutoReauthnPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentityAutoReauthnPermissionContextFactory::
    FederatedIdentityAutoReauthnPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "FederatedIdentityAutoReauthnPermissionContext",
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
}

FederatedIdentityAutoReauthnPermissionContextFactory::
    ~FederatedIdentityAutoReauthnPermissionContextFactory() = default;

std::unique_ptr<KeyedService>
FederatedIdentityAutoReauthnPermissionContextFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* profile) const {
  return std::make_unique<FederatedIdentityAutoReauthnPermissionContext>(
      profile);
}
