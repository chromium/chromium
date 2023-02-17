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
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityAutoReauthnPermissionContextFactory::
    ~FederatedIdentityAutoReauthnPermissionContextFactory() = default;

KeyedService*
FederatedIdentityAutoReauthnPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityAutoReauthnPermissionContext(profile);
}
