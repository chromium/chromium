// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_permission_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"

// static
FederatedIdentityPermissionContext*
FederatedIdentityPermissionContextFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<FederatedIdentityPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
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
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

FederatedIdentityPermissionContextFactory::
    ~FederatedIdentityPermissionContextFactory() = default;

KeyedService*
FederatedIdentityPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FederatedIdentityPermissionContext(profile);
}

void FederatedIdentityPermissionContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  GetForProfile(Profile::FromBrowserContext(context))
      ->FlushScheduledSaveSettingsCalls();
}
