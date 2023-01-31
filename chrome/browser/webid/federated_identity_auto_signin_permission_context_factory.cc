// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_signin_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_auto_signin_permission_context.h"

// static
FederatedIdentityAutoSigninPermissionContext*
FederatedIdentityAutoSigninPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityAutoSigninPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FederatedIdentityAutoSigninPermissionContextFactory*
FederatedIdentityAutoSigninPermissionContextFactory::GetInstance() {
  static base::NoDestructor<FederatedIdentityAutoSigninPermissionContextFactory>
      instance;
  return instance.get();
}

FederatedIdentityAutoSigninPermissionContextFactory::
    FederatedIdentityAutoSigninPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "FederatedIdentityAutoSigninPermissionContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityAutoSigninPermissionContextFactory::
    ~FederatedIdentityAutoSigninPermissionContextFactory() = default;

KeyedService*
FederatedIdentityAutoSigninPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityAutoSigninPermissionContext(profile);
}
