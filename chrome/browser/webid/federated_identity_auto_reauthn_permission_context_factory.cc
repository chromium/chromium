// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
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
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(PermissionDecisionAutoBlockerFactory::GetInstance());
}

FederatedIdentityAutoReauthnPermissionContextFactory::
    ~FederatedIdentityAutoReauthnPermissionContextFactory() = default;

std::unique_ptr<KeyedService>
FederatedIdentityAutoReauthnPermissionContextFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return std::make_unique<FederatedIdentityAutoReauthnPermissionContext>(
      HostContentSettingsMapFactory::GetForProfile(profile),
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile));
}
